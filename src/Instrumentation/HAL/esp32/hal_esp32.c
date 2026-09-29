/*
 * ESP32 HAL instrumentation
 * Fair warning: this is a bit of a mess, some sensors work and others dont
 * it needs a proper debugging session to iron out issues, might delegate it to an intern >:)
*/
#include "hal.h"

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#include "can_bus.h"
#include "pins.h" 

static const char *TAG = "instr_hal";

// I've double checked these should be right, might move to pins.h later? 
#define SDA_GPIO 8
#define SCL_GPIO 9
#define I2C_HZ   100000
#define I2C_PORT I2C_NUM_0

#define ADDR_SHT45     0x44
#define ADDR_ADS1115   0x48
#define ADDR_FSP201    0x4C 
#define ADDR_SCD41     0x62
#define ADDR_MS5611_A  0x76 
#define ADDR_MS5611_B  0x77 

static bool s_ok_sht45, s_ok_ads1115, s_ok_imu, s_ok_scd41, s_ok_ms5611;
static uint8_t s_ms5611_addr;
static uint16_t s_ms5611_c[6]; 

static hal_imu_t s_imu_latest;


static bool i2c_probe(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

static bool i2c_write(uint8_t addr, const uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    if (len) i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK;
}

static bool i2c_read(uint8_t addr, uint8_t *data, size_t len) {
    if (len == 0) return true;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_READ), true);

    if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK;
}


static bool i2c_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len, uint8_t *rd, size_t rd_len) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    if (wr_len) i2c_master_write(cmd, (uint8_t *)wr, wr_len, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_READ), true);

    if (rd_len > 1) i2c_master_read(cmd, rd, rd_len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, rd + rd_len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err == ESP_OK;
}

static uint8_t sensirion_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}



hal_sht45_t hal_read_sht45(void) {
    hal_sht45_t r = {0};
    if (!s_ok_sht45) return r;

    uint8_t cmd = 0xFD; 
    if (!i2c_write(ADDR_SHT45, &cmd, 1)) return r;
    vTaskDelay(pdMS_TO_TICKS(10)); /

    uint8_t buf[6];
    if (!i2c_read(ADDR_SHT45, buf, sizeof buf)) return r;
    if (sensirion_crc8(&buf[0], 2) != buf[2] || sensirion_crc8(&buf[3], 2) != buf[5]) {
        ESP_LOGW(TAG, "SHT45 CRC mismatch");
        return r;
    }

    uint16_t raw_t  = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_rh = ((uint16_t)buf[3] << 8) | buf[4];
    float temp_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    float rh_pct = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);
    if (rh_pct < 0.0f) rh_pct = 0.0f;
    if (rh_pct > 100.0f) rh_pct = 100.0f;

    r.ok         = true;
    r.temp_c_x10 = (int16_t)lroundf(temp_c * 10.0f);
    r.rh_pct_x10 = (int16_t)lroundf(rh_pct * 10.0f);

    return r;
}

hal_ads1115_t hal_read_ads1115(void) {
    hal_ads1115_t r = {0};
    if (!s_ok_ads1115) return r;
    r.ok = true;

    for (int ch = 0; ch < 4; ch++) {
        uint16_t mux = (uint16_t)(0x4 + ch);
        uint16_t config = (uint16_t)(
            (1u << 15) | (mux << 12) | (0x1u << 9) | (1u << 8) | (0x4u << 5) | 0x3u);
        uint8_t wr[3] = {0x01, (uint8_t)(config >> 8), (uint8_t)(config & 0xFF)};
        if (!i2c_write(ADDR_ADS1115, wr, sizeof wr)) {
            r.ok = false;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 

        uint8_t ptr = 0x00, rd[2];
        if (!i2c_write_read(ADDR_ADS1115, &ptr, 1, rd, sizeof rd)) {
            r.ok = false;
            continue;
        }
        int16_t raw = (int16_t)(((uint16_t)rd[0] << 8) | rd[1]);
        r.mv[ch] = (int16_t)lroundf(raw * 0.125f); 
    }
    return r;
}


static bool ms5611_read_adc(uint8_t convert_cmd, uint32_t *out) {
    // TODO(remy): double check with Chrysler
}

hal_ms5611_t hal_read_ms5611(void) {
    // TODO
}

static bool scd41_send_cmd(uint16_t cmd)
{
    uint8_t wr[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return i2c_write(ADDR_SCD41, wr, sizeof wr);
}

hal_scd41_t hal_read_scd41(void)
{
    hal_scd41_t r = {0};
    if (!s_ok_scd41) return r;

    if (!scd41_send_cmd(0xE4B8)) return r; /* get_data_ready_status */
    vTaskDelay(pdMS_TO_TICKS(2));
    uint8_t rdy[3];
    if (!i2c_read(ADDR_SCD41, rdy, sizeof rdy)) return r;
    uint16_t status = ((uint16_t)rdy[0] << 8) | rdy[1];
    if ((status & 0x07FFu) == 0) return r; /* not ready yet - not an error */

    if (!scd41_send_cmd(0xEC05)) return r; /* read_measurement */
    vTaskDelay(pdMS_TO_TICKS(2));
    uint8_t buf[9];
    if (!i2c_read(ADDR_SCD41, buf, sizeof buf)) return r;
    if (sensirion_crc8(&buf[0], 2) != buf[2] ||
        sensirion_crc8(&buf[3], 2) != buf[5] ||
        sensirion_crc8(&buf[6], 2) != buf[8]) {
        ESP_LOGW(TAG, "SCD41 CRC mismatch");
        return r;
    }

    uint16_t co2    = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_t  = ((uint16_t)buf[3] << 8) | buf[4];
    uint16_t raw_rh = ((uint16_t)buf[6] << 8) | buf[7];
    float temp_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    float rh_pct = 100.0f * ((float)raw_rh / 65535.0f);

    r.ok         = true;
    r.co2_ppm    = co2;
    r.temp_c_x10 = (int16_t)lroundf(temp_c * 10.0f);
    r.rh_pct_x10 = (int16_t)lroundf(rh_pct * 10.0f);
    return r;
}

/* ---- BNO08x (IMU, accelerometer only) - minimal SHTP+SH2, see file header
 * caveat. Channel 2 = control (used to enable the report), channel 3 =
 * input sensor reports (polled for the report itself). ------------------- */

#define SHTP_MAX_PACKET 64
#define SH2_CHAN_CONTROL 2
#define SH2_CHAN_INPUT   3
#define SH2_REPORT_SET_FEATURE 0xFD
#define SH2_SENSOR_ACCELEROMETER 0x01

static bool imu_send_set_feature(uint8_t report_id, uint32_t interval_us)
{
    uint8_t payload[17] = {0};
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

/* Diagnostic-only: dump the first few raw SHTP packets so we can see what
 * the BNO08x is actually sending, since the accelerometer report is not
 * yet being successfully parsed (imu=- in the periodic summary) despite
 * the enable-report write succeeding. Remove once hal_poll_imu() is
 * confirmed working against real hardware. */
static int s_imu_dump_budget = 8;

static void imu_dump_packet(const uint8_t *buf, uint16_t len, uint8_t channel)
{
    if (s_imu_dump_budget <= 0) return;
    s_imu_dump_budget--;

    char hex[3 * 20 + 1] = {0};
    int n = (len < 20) ? len : 20;
    for (int i = 0; i < n; i++) {
        snprintf(&hex[i * 3], 4, "%02X ", buf[i]);
    }
    ESP_LOGI(TAG, "IMU pkt: len=%u chan=%u bytes=%s", len, channel, hex);
}

static void hal_poll_imu(void)
{
    if (!s_ok_imu) return;

    uint8_t buf[SHTP_MAX_PACKET];
    if (!i2c_read(ADDR_FSP201, buf, sizeof buf)) return;

    uint16_t len = (uint16_t)(buf[0] | ((buf[1] & 0x7Fu) << 8));
    if (len < 4 || len > sizeof buf) {
        if (len != 0) imu_dump_packet(buf, sizeof buf, buf[2]); /* garbage/oversized - still worth seeing */
        return; /* nothing pending / garbage */
    }
    uint8_t channel = buf[2];
    imu_dump_packet(buf, len, channel);
    if (channel != SH2_CHAN_INPUT) return;

    for (uint16_t i = 4; i + 10 <= len; i++) {
        if (buf[i] != SH2_SENSOR_ACCELEROMETER) continue;
        int16_t x = (int16_t)(buf[i + 4] | (buf[i + 5] << 8));
        int16_t y = (int16_t)(buf[i + 6] | (buf[i + 7] << 8));
        int16_t z = (int16_t)(buf[i + 8] | (buf[i + 9] << 8));
        s_imu_latest.ok      = true;
        s_imu_latest.ax_mmss = (int16_t)lroundf(x * 3.90625f); /* Q8: raw/256 m/s^2 -> mm/s^2 */
        s_imu_latest.ay_mmss = (int16_t)lroundf(y * 3.90625f);
        s_imu_latest.az_mmss = (int16_t)lroundf(z * 3.90625f);
        break;
    }
}

hal_imu_t hal_read_imu(void)
{
    hal_poll_imu();
    return s_imu_latest;
}

/* ---- init / restart ------------------------------------------------------ */

bool hal_init(void)
{
    if (!can_bus_init(PIN_CAN_TX, PIN_CAN_RX)) return false;

    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SDA_GPIO,
        .scl_io_num       = SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_DISABLE, /* breakouts have their own external pull-ups */
        .scl_pullup_en    = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_HZ,
    };
    if (i2c_param_config(I2C_PORT, &conf) != ESP_OK ||
        i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed - sensors unavailable this boot");
        return true; /* CAN is up, which is what's fatal */
    }

    /* SCD41 needs >=1s after power-up before it will ACK on the bus at all
     * (Sensirion datasheet). Everything else responds immediately, but this
     * delay is cheap and shared across all sensors, so pay it once here
     * rather than special-casing SCD41's probe. */
    vTaskDelay(pdMS_TO_TICKS(1000));

    {
        int found = 0;
        ESP_LOGI(TAG, "I2C scan:");
        for (uint8_t addr = 1; addr < 127; addr++) {
            if (i2c_probe(addr)) {
                ESP_LOGI(TAG, "  found 0x%02X", addr);
                found++;
            }
        }
        if (found == 0) {
            ESP_LOGW(TAG, "  nothing found - check wiring/pull-ups/SDA=%d,SCL=%d before trusting the per-sensor results below", SDA_GPIO, SCL_GPIO);
        }
    }

    s_ok_sht45   = i2c_probe(ADDR_SHT45);
    s_ok_ads1115 = i2c_probe(ADDR_ADS1115);
    s_ok_imu     = i2c_probe(ADDR_FSP201);
    s_ok_scd41   = i2c_probe(ADDR_SCD41);

    /* MS5611's address depends on how its CSB pin is wired (0x76 if tied
     * high, 0x77 if tied low) - try both rather than assuming. */
    if (i2c_probe(ADDR_MS5611_A)) {
        s_ms5611_addr = ADDR_MS5611_A;
        s_ok_ms5611 = true;
    } else if (i2c_probe(ADDR_MS5611_B)) {
        s_ms5611_addr = ADDR_MS5611_B;
        s_ok_ms5611 = true;
    } else {
        s_ok_ms5611 = false;
    }

    if (s_ok_scd41) {
        scd41_send_cmd(0x3F86); /* stop_periodic_measurement, in case it was left running */
        vTaskDelay(pdMS_TO_TICKS(500));
        scd41_send_cmd(0x3646); /* reinit */
        vTaskDelay(pdMS_TO_TICKS(30));
        s_ok_scd41 = scd41_send_cmd(0x21B1); /* start_periodic_measurement */
    }

    if (s_ok_ms5611) {
        uint8_t reset = 0x1E;
        i2c_write(s_ms5611_addr, &reset, 1);
        vTaskDelay(pdMS_TO_TICKS(3));
        for (int i = 0; i < 6; i++) {
            uint8_t cmd = (uint8_t)(0xA2 + i * 2);
            uint8_t rd[2];
            if (!i2c_write_read(s_ms5611_addr, &cmd, 1, rd, sizeof rd)) {
                s_ok_ms5611 = false;
                break;
            }
            s_ms5611_c[i] = (uint16_t)(((uint16_t)rd[0] << 8) | rd[1]);
        }
    }

    if (s_ok_imu) {
        vTaskDelay(pdMS_TO_TICKS(100)); /* let the reset/advertisement packets settle */
        s_ok_imu = imu_send_set_feature(SH2_SENSOR_ACCELEROMETER, 100000 /* 10 Hz */);
    }

    ESP_LOGI(TAG, "SHT45   %s", s_ok_sht45   ? "OK" : "NOT FOUND");
    ESP_LOGI(TAG, "ADS1115 %s", s_ok_ads1115 ? "OK" : "NOT FOUND");
    ESP_LOGI(TAG, "SCD41   %s", s_ok_scd41   ? "OK" : "NOT FOUND");
    if (s_ok_ms5611) {
        ESP_LOGI(TAG, "MS5611  OK (0x%02X)", s_ms5611_addr);
    } else {
        ESP_LOGI(TAG, "MS5611  NOT FOUND");
    }
    ESP_LOGI(TAG, "FSP201  %s", s_ok_imu     ? "OK" : "NOT FOUND");

    return true;
}

void hal_restart(void)
{
    esp_restart();
}