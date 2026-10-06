/**
 * @file hal_imu.c
 * @brief BNO08x-family IMU ("FSP201", I2C addr 0x4C), accelerometer
 *        report only. Minimal SHTP/SH2 framing - channel 2 is the
 *        control channel (used to send the enable-report command),
 *        channel 3 carries input sensor reports (polled for the report
 *        itself).
 *
 * @warning Unconfirmed against real hardware: the only packet ever seen
 *          on the bench is the channel-0 advertisement the chip sends
 *          unsolicited right after reset - never a channel-3 input
 *          report, across 20+ seconds of polling with every other
 *          sensor on the bus healthy. The enable-report write does ACK
 *          on the bus, but that only means the I2C transaction
 *          succeeded, not that the chip accepted the command while it
 *          was still mid-boot.
 *
 *          Working theory: the original code sent that command exactly
 *          once, 100ms after probe - if the chip hadn't finished its
 *          internal boot by then, the command is silently dropped and
 *          nothing ever retries it. This version resends it periodically
 *          (every IMU_RETRY_MS) until a channel-3 report is actually
 *          observed, which removes the single-shot timing race without
 *          needing to know the exact boot duration. If reports still
 *          never show up with retries in place, that points at FSP201
 *          needing its INT/HINT pin watched rather than blind-polled -
 *          worth checking whether that pin exists and is wired.
 */
#include "hal.h"
#include "hal_internal.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "instr_hal_imu";

#define ADDR_FSP201 0x4C

#define SHTP_MAX_PACKET 64
#define SH2_CHAN_CONTROL 2
#define SH2_CHAN_INPUT 3
#define SH2_REPORT_SET_FEATURE 0xFD
#define SH2_SENSOR_ACCELEROMETER 0x01
#define IMU_REPORT_INTERVAL_US 100000u /* 10 Hz */
#define IMU_RETRY_MS 2000u             /* resend enable-report until it actually works */

static bool s_ok;
static hal_imu_t s_latest;
static uint32_t s_next_retry_ms;

/* Diagnostic-only: dump the first few raw SHTP packets so we can see
 * what the chip is actually sending. Remove once hal_read_imu() is
 * confirmed working against real hardware. */
static int s_dump_budget = 16;

static uint32_t now_ms(void) {
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static bool imu_send_set_feature(uint8_t report_id, uint32_t interval_us) {
    uint8_t payload[17] = { 0 };
    payload[0] = SH2_REPORT_SET_FEATURE;
    payload[1] = report_id;
    payload[5] = (uint8_t)(interval_us & 0xFF);
    payload[6] = (uint8_t)((interval_us >> 8) & 0xFF);
    payload[7] = (uint8_t)((interval_us >> 16) & 0xFF);
    payload[8] = (uint8_t)((interval_us >> 24) & 0xFF);

    uint16_t total = 4 + sizeof payload;
    uint8_t frame[4 + sizeof payload];
    frame[0] = (uint8_t)(total & 0xFF);
    frame[1] = (uint8_t)((total >> 8) & 0x7F);
    frame[2] = SH2_CHAN_CONTROL;
    frame[3] = 0; /* sequence number - not tracked, best-effort */
    memcpy(&frame[4], payload, sizeof payload);

    return i2c_write(ADDR_FSP201, frame, sizeof frame);
}

static void imu_dump_packet(const uint8_t *buf, uint16_t len, uint8_t channel) {
    if (s_dump_budget <= 0)
        return;
    s_dump_budget--;

    char hex[3 * 20 + 1] = { 0 };
    int n = (len < 20) ? len : 20;
    for (int i = 0; i < n; i++) {
        snprintf(&hex[i * 3], 4, "%02X ", buf[i]);
    }
    ESP_LOGI(TAG, "pkt: len=%u chan=%u bytes=%s", len, channel, hex);
}

bool hal_imu_probe(void) {
    s_ok = i2c_probe(ADDR_FSP201);
    if (!s_ok)
        return false;

    vTaskDelay(pdMS_TO_TICKS(250)); /* let reset/advertisement packets settle */
    imu_send_set_feature(SH2_SENSOR_ACCELEROMETER, IMU_REPORT_INTERVAL_US);
    s_next_retry_ms = now_ms() + IMU_RETRY_MS;
    /* Don't gate s_ok on this write's return value - an ACK'd write is no
     * guarantee the chip actually applied it (see file header), so a
     * false here would permanently disable the retries below over a
     * false negative. Presence (the probe above) is the real gate. */
    return s_ok;
}

static void hal_poll_imu(void) {
    if (!s_ok)
        return;

    /* Keep resending the enable-report command until we've actually seen
     * a channel-3 report - guards against the command landing in a dead
     * window during the chip's own boot sequence, which an I2C ACK can't
     * distinguish from a real success. */
    if (!s_latest.ok) {
        uint32_t t = now_ms();
        if ((int32_t)(t - s_next_retry_ms) >= 0) {
            ESP_LOGI(TAG, "no report yet - resending enable-report");
            imu_send_set_feature(SH2_SENSOR_ACCELEROMETER, IMU_REPORT_INTERVAL_US);
            s_next_retry_ms = t + IMU_RETRY_MS;
        }
    }

    uint8_t buf[SHTP_MAX_PACKET];
    if (!i2c_read(ADDR_FSP201, buf, sizeof buf))
        return;

    uint16_t len = (uint16_t)(buf[0] | ((buf[1] & 0x7Fu) << 8));
    if (len < 4 || len > sizeof buf) {
        if (len != 0)
            imu_dump_packet(buf, sizeof buf, buf[2]); /* garbage/oversized - still worth seeing */
        return;                                       /* nothing pending / garbage */
    }
    uint8_t channel = buf[2];
    imu_dump_packet(buf, len, channel);
    if (channel != SH2_CHAN_INPUT)
        return;

    for (uint16_t i = 4; i + 10 <= len; i++) {
        if (buf[i] != SH2_SENSOR_ACCELEROMETER)
            continue;
        int16_t x = (int16_t)(buf[i + 4] | (buf[i + 5] << 8));
        int16_t y = (int16_t)(buf[i + 6] | (buf[i + 7] << 8));
        int16_t z = (int16_t)(buf[i + 8] | (buf[i + 9] << 8));
        s_latest.ok = true; /* once true, retries above stop permanently */
        s_latest.ax_mmss = (int16_t)lroundf(x * 3.90625f); /* Q8: raw/256 m/s^2 -> mm/s^2 */
        s_latest.ay_mmss = (int16_t)lroundf(y * 3.90625f);
        s_latest.az_mmss = (int16_t)lroundf(z * 3.90625f);
        break;
    }
}

hal_imu_t hal_read_imu(void) {
    hal_poll_imu();
    return s_latest;
}