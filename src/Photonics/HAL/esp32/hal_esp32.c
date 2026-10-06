/* Photonics HAL: CAN bus + a TCA9548A I2C mux carrying however many
 * LTR390 UV/ambient-light sensors and ADS1115 flask-photodiode ADCs are
 * actually wired up right now. Ported from the Arduino prototype's
 * per-channel auto-detect scan (see file header in hal.h for why fixed
 * channel assignments stopped making sense once the board grew past the
 * original 2-UV-sensor-plus-onboard-ADC bring-up test), onto the same
 * legacy driver/i2c.h API Instrumentation's hal_esp32.c uses - that's the
 * one confirmed working on this hardware/IDF combo.
 *
 * Detection happens once, at boot, same as every other node's HAL - a
 * flask plugged in after boot won't show up until the next restart. Fine
 * for a bench/lab-integration board; revisit if flasks ever need to be
 * hot-swapped mid-run.
 *
 * SDA/SCL and both sensor addresses are carried over unchanged from the
 * previous revision - confirm against the Photonics Rev B netlist if
 * that's still outstanding.
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
#include "pins.h" /* PIN_CAN_TX / PIN_CAN_RX */

static const char *TAG = "photonics_hal";

/* ---- Pins / addresses - confirm against Rev B netlist ----------------- */
#define SDA_GPIO 8
#define SCL_GPIO 9
#define I2C_HZ   100000
#define I2C_PORT I2C_NUM_0

#define ADDR_MUX     0x70 /* TCA9548A, A0-A2 tied low (default) */
#define ADDR_LTR390  0x53
#define ADDR_ADS1115 0x48

#define MUX_SETTLE_MS 2 /* matches the Arduino prototype's post-select delay */

static bool s_mux_ok;

/* Which physical mux channel each discovered UV sensor / flask ADC lives
 * behind - filled in by the boot scan, not assumed. */
static uint8_t s_uv_channel[PHOTONICS_MAX_UV];
static uint8_t s_uv_count;
static hal_uv_t s_uv_latest[PHOTONICS_MAX_UV];

typedef enum { LTR390_MODE_ALS, LTR390_MODE_UVS } ltr390_mode_t;
static ltr390_mode_t s_uv_mode[PHOTONICS_MAX_UV];

static uint8_t s_flask_channel[PHOTONICS_MAX_FLASKS];
static uint8_t s_flask_count;

/* ---- Legacy i2c.h transaction helpers - identical to Instrumentation's -- */

static bool i2c_probe(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

static bool i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    if (len) i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

static bool i2c_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len, uint8_t *rd, size_t rd_len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    if (wr_len) i2c_master_write(cmd, (uint8_t *)wr, wr_len, true);
    i2c_master_start(cmd); /* repeated start */
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_READ), true);
    if (rd_len > 1) i2c_master_read(cmd, rd, rd_len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, rd + rd_len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t wr[2] = {reg, val};
    return i2c_write(addr, wr, sizeof wr);
}

static bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_write_read(addr, &reg, 1, buf, len);
}

/* ---- TCA9548A mux -------------------------------------------------------- */

static bool mux_select(uint8_t channel)
{
    uint8_t bit = (uint8_t)(1u << channel);
    return i2c_write(ADDR_MUX, &bit, 1);
}

/* ---- Per-channel detection, same two checks as the Arduino prototype --- */

#define LTR390_REG_PART_ID 0x06

/* LTR390 PART_ID register: upper nibble should read 0xB on a real part -
 * a plain i2c_probe() ack isn't enough to tell "LTR390 here" apart from
 * "something else happens to be at 0x53", same reasoning as the prototype's
 * ltrAvailable(). */
static bool ltr_available(void)
{
    if (!i2c_probe(ADDR_LTR390)) return false;
    uint8_t part_id = 0;
    if (!i2c_read_reg(ADDR_LTR390, LTR390_REG_PART_ID, &part_id, 1)) return false;
    return (part_id >> 4) == 0xB;
}

static bool adc_available(void)
{
    return i2c_probe(ADDR_ADS1115);
}

/* ---- LTR390 (UV + ambient light) -----------------------------------------
 * Register map, from the datasheet:
 *   0x00 MAIN_CTRL   bit1=SENSOR_EN, bit3=MODE(0=ALS,1=UVS), bit4=SW_RESET
 *   0x04 MEAS_RATE    bits[6:4]=resolution, bits[2:0]=measurement rate
 *   0x05 GAIN         bits[2:0]: 0=1x 1=3x 2=6x 3=9x 4=18x
 *   0x06 PART_ID      bits[7:4] = 0xB on a real LTR390
 *   0x07 MAIN_STATUS  bit3=new-data-available
 *   0x0D ALS_DATA0    3 bytes LE, 20-bit
 *   0x10 UVS_DATA0    3 bytes LE, 20-bit
 *
 * Ping-pongs between the same two configs the bring-up test used:
 *   ALS: gain 3x, 18-bit res  -> Lux = 0.6 * raw / gain / int, int=1 @ 18-bit
 *   UVS: gain 18x, 20-bit res -> UVI = raw / 2300 (datasheet's recommended
 *                                       combo for UV index)
 * -------------------------------------------------------------------------- */

#define LTR390_REG_MAIN_CTRL   0x00
#define LTR390_REG_MEAS_RATE   0x04
#define LTR390_REG_GAIN        0x05
#define LTR390_REG_MAIN_STATUS 0x07
#define LTR390_REG_ALS_DATA0   0x0D
#define LTR390_REG_UVS_DATA0   0x10

static bool ltr390_set_als(void)
{
    bool ok = true;
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_GAIN, 0x01);      /* 3x */
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MEAS_RATE, 0x22); /* 18-bit / 100ms */
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x02); /* EN + ALS */
    return ok;
}

static bool ltr390_set_uvs(void)
{
    bool ok = true;
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_GAIN, 0x04);      /* 18x */
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MEAS_RATE, 0x02); /* 20-bit / 100ms */
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x0A); /* EN + UVS */
    return ok;
}

/* Caller has already mux_select()'d onto this sensor's channel. */
static bool ltr390_init_on_selected_channel(uint8_t slot)
{
    i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x10); /* SW_RESET */
    vTaskDelay(pdMS_TO_TICKS(10));

    if (!ltr390_set_als()) return false;
    s_uv_mode[slot] = LTR390_MODE_ALS;
    return true;
}

static uint32_t ltr390_read_data(uint8_t reg)
{
    uint8_t buf[3] = {0};
    if (!i2c_read_reg(ADDR_LTR390, reg, buf, sizeof buf)) return 0;
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)(buf[2] & 0x0F) << 16);
}

hal_uv_t hal_read_uv(uint8_t idx)
{
    hal_uv_t r = {0};
    if (idx >= s_uv_count) return r;

    r = s_uv_latest[idx];
    r.fresh = false;

    /* Must select before ANY call to this sensor, including the status
     * read below - same as the original bring-up test's comment. */
    mux_select(s_uv_channel[idx]);

    uint8_t status = 0;
    if (!i2c_read_reg(ADDR_LTR390, LTR390_REG_MAIN_STATUS, &status, 1)) return r;
    if (!(status & 0x08)) return r; /* no new data yet - normal, not an error */

    if (s_uv_mode[idx] == LTR390_MODE_ALS) {
        uint32_t raw = ltr390_read_data(LTR390_REG_ALS_DATA0);
        float lux = 0.6f * (float)raw / 3.0f; /* gain=3x, int=1 @ 18-bit */
        r.lux_x10 = (int32_t)lroundf(lux * 10.0f);
        ltr390_set_uvs();
        s_uv_mode[idx] = LTR390_MODE_UVS;
    } else {
        uint32_t raw = ltr390_read_data(LTR390_REG_UVS_DATA0);
        float uvi = (float)raw / 2300.0f; /* gain=18x, 20-bit - datasheet's UVI combo */
        r.uvi_x1000 = (int32_t)lroundf(uvi * 1000.0f);
        ltr390_set_als();
        s_uv_mode[idx] = LTR390_MODE_ALS;
    }

    r.ok = true;
    r.fresh = true;
    s_uv_latest[idx] = r;
    return r;
}

/* ---- ADS1115 (4-channel ADC, one per flask) ----------------------------- */

hal_flask_t hal_read_flask(uint8_t idx)
{
    hal_flask_t r = {0};
    if (idx >= s_flask_count) return r;
    r.ok = true;

    mux_select(s_flask_channel[idx]);

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
        /* LSB = 4.096V / 32768 = 0.125mV, GAIN_ONE - same as before; worth
         * revisiting once there's a known photodiode signal range to tune
         * the gain/FSR against. */
        r.mv[ch] = (int16_t)lroundf(raw * 0.125f);
    }
    return r;
}

/* ---- init / restart ------------------------------------------------------ */

uint8_t hal_num_uv(void) { return s_uv_count; }
uint8_t hal_num_flasks(void) { return s_flask_count; }

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

    s_mux_ok = i2c_probe(ADDR_MUX);
    ESP_LOGI(TAG, "TCA9548A mux %s", s_mux_ok ? "OK" : "NOT FOUND");
    if (!s_mux_ok) {
        ESP_LOGW(TAG, "mux not found - no UV sensors or flask ADCs reachable this boot");
        return true;
    }

    /* Scan every mux channel and sort whatever answers into a UV slot or a
     * flask slot, same idea as the Arduino prototype's per-channel
     * ltrAvailable()/adcAvailable() check - this is what lets electrical
     * add/move flasks without any code change here. */
    for (uint8_t ch = 0; ch < PHOTONICS_MUX_CHANNELS; ch++) {
        mux_select(ch);
        vTaskDelay(pdMS_TO_TICKS(MUX_SETTLE_MS));

        if (ltr_available()) {
            if (s_uv_count < PHOTONICS_MAX_UV) {
                uint8_t slot = s_uv_count++;
                s_uv_channel[slot] = ch;
                bool ok = ltr390_init_on_selected_channel(slot);
                ESP_LOGI(TAG, "mux ch %u: LTR390 -> UV slot %u %s", ch, slot, ok ? "OK" : "INIT FAILED");
            } else {
                ESP_LOGW(TAG, "mux ch %u: LTR390 found but all %u UV slots are full - ignoring",
                          ch, PHOTONICS_MAX_UV);
            }
        } else if (adc_available()) {
            if (s_flask_count < PHOTONICS_MAX_FLASKS) {
                uint8_t slot = s_flask_count++;
                s_flask_channel[slot] = ch;
                ESP_LOGI(TAG, "mux ch %u: ADS1115 -> flask slot %u", ch, slot);
            } else {
                ESP_LOGW(TAG, "mux ch %u: ADS1115 found but all %u flask slots are full - ignoring",
                          ch, PHOTONICS_MAX_FLASKS);
            }
        } else {
            ESP_LOGI(TAG, "mux ch %u: nothing found", ch);
        }
    }

    ESP_LOGI(TAG, "boot scan done: %u/%u UV sensor(s), %u/%u flask ADC(s) found",
              s_uv_count, PHOTONICS_MAX_UV, s_flask_count, PHOTONICS_MAX_FLASKS);

    return true;
}

void hal_restart(void)
{
    esp_restart();
}