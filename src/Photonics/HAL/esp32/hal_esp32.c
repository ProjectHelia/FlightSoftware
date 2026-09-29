/*

*/
#include "hal.h"

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Driver docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html#_CPPv4N3i2c12i2c_master_cmd_beginEii
#include "driver/i2c.h"

#include "can_bus.h"
#include "pins.h" 

static const char *TAG = "photonics_hal";

#define SDA_GPIO 8
#define SCL_GPIO 9
#define I2C_HZ   100000
#define I2C_PORT I2C_NUM_0

#define ADDR_MUX     0x70 // Check data sheet for TCA9548A, this is the default address with A0,A1,A2 = GND
#define ADDR_LTR390  0x53
#define ADDR_ADS1115 0x48

static const uint8_t UV_MUX_CHANNEL[PHOTONICS_NUM_UV] = {0, 1}; 

static bool s_mux_ok;

static bool s_uv_ok[PHOTONICS_NUM_UV];
static hal_uv_t s_uv_latest[PHOTONICS_NUM_UV];

typedef enum { LTR390_MODE_ALS, LTR390_MODE_UVS } ltr390_mode_t;
static ltr390_mode_t s_uv_mode[PHOTONICS_NUM_UV];

static bool s_adc_ok; // true if the onboard ADS1115 was found at init, useful for debugging

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

static bool i2c_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len, uint8_t *rd, size_t rd_len) {
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

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t wr[2] = {reg, val};

    return i2c_write(addr, wr, sizeof wr);
}

static bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_write_read(addr, &reg, 1, buf, len);
}

static bool mux_select(uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);

    return i2c_write(ADDR_MUX, &bit, 1);
}

#define LTR390_REG_MAIN_CTRL   0x00 
#define LTR390_REG_MEAS_RATE   0x04
#define LTR390_REG_GAIN        0x05
#define LTR390_REG_MAIN_STATUS 0x07
#define LTR390_REG_ALS_DATA0   0x0D
#define LTR390_REG_UVS_DATA0   0x10

static bool ltr390_set_als(void) {
    bool ok = true;

    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_GAIN, 0x01);   
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MEAS_RATE, 0x22); 
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x02); 

    return ok;
}

static bool ltr390_set_uvs(void) {
    bool ok = true;

    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_GAIN, 0x04); 
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MEAS_RATE, 0x02);
    ok &= i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x0A); 

    return ok;
}

static bool ltr390_init_one(uint8_t idx) {
    mux_select(UV_MUX_CHANNEL[idx]);
    if (!i2c_probe(ADDR_LTR390)) return false;

    i2c_write_reg(ADDR_LTR390, LTR390_REG_MAIN_CTRL, 0x10); 
    vTaskDelay(pdMS_TO_TICKS(10));

    if (!ltr390_set_als()) return false;
    s_uv_mode[idx] = LTR390_MODE_ALS;

    return true;
}

static uint32_t ltr390_read_data(uint8_t reg){
    uint8_t buf[3] = {0};
    if (!i2c_read_reg(ADDR_LTR390, reg, buf, sizeof buf)) return 0;

    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)(buf[2] & 0x0F) << 16);
}

hal_uv_t hal_read_uv(uint8_t idx) {
    hal_uv_t r = {0};
    if (idx >= PHOTONICS_NUM_UV || !s_mux_ok || !s_uv_ok[idx]) return r;

    r = s_uv_latest[idx];
    r.fresh = false;

    mux_select(UV_MUX_CHANNEL[idx]);

    uint8_t status = 0;
    if (!i2c_read_reg(ADDR_LTR390, LTR390_REG_MAIN_STATUS, &status, 1)) return r;
    if (!(status & 0x08)) return r; 

    // Read the data from the sensor, convert to lux or UVI, and flip the mode for next time
    // Flipping the mode is a bit weird but the original photonics code does it? 
    // TODO(remy) double check with Kyane
    if (s_uv_mode[idx] == LTR390_MODE_ALS) {
        uint32_t raw = ltr390_read_data(LTR390_REG_ALS_DATA0);
        float lux = 0.6f * (float)raw / 3.0f; 
        r.lux_x10 = (int32_t)lroundf(lux * 10.0f);
        ltr390_set_uvs();
        s_uv_mode[idx] = LTR390_MODE_UVS;

    } else {
        uint32_t raw = ltr390_read_data(LTR390_REG_UVS_DATA0);
        float uvi = (float)raw / 2300.0f; 
        r.uvi_x1000 = (int32_t)lroundf(uvi * 1000.0f);
        ltr390_set_als();
        s_uv_mode[idx] = LTR390_MODE_ALS;

    }

    r.ok = true;
    r.fresh = true;
    s_uv_latest[idx] = r;

    return r;
}

// Literally ctrl-c and ctrl-v from Instrumentation's hal.c, just with the ADS1115 address changed :3
hal_adc_t hal_read_adc(void) {
    hal_adc_t r = {0};
    if (!s_adc_ok) return r;
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

        // The 0.125f is to convert from raw counts to millivolts, based on the ADS1115 datasheet for the selected gain
        // ADS1115 datasheet: https://www.ti.com/lit/ds/symlink/ads1115.pdf
        r.mv[ch] = (int16_t)lroundf(raw * 0.125f); 
    }
    return r;
}

bool hal_init(void) {
    if (!can_bus_init(PIN_CAN_TX, PIN_CAN_RX)) return false;

    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SDA_GPIO,
        .scl_io_num       = SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_DISABLE, 
        .scl_pullup_en    = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_HZ,
    };

    if (i2c_param_config(I2C_PORT, &conf) != ESP_OK ||
        i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed!!! sensors unavailable at boot");

        // still boot just with broken sensors
        return true; 
    }

    int found = 0;
    ESP_LOGI(TAG, "I2C scan (main bus, mux not yet selected):");
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_probe(addr)) {
            ESP_LOGI(TAG, "==> found 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "==> nothing found :/ double check shit before trusting sensor values(SDA=%d,SCL=%d) ", SDA_GPIO, SCL_GPIO);
    }
    ESP_LOGI(TAG, "I2C scan done, %d device(s) found", found);

    s_mux_ok = i2c_probe(ADDR_MUX);
    ESP_LOGI(TAG, "TCA9548A mux %s", s_mux_ok ? "OK" : "NOT FOUND");

    for (uint8_t i = 0; i < PHOTONICS_NUM_UV; i++) {
        s_uv_ok[i] = s_mux_ok && ltr390_init_one(i);
        ESP_LOGI(TAG, "LTR390 %u (mux ch %u) %s", i, UV_MUX_CHANNEL[i], s_uv_ok[i] ? "OK" : "NOT FOUND");
    }

    // Defaults to channel 0 on boot, this is just a placeholder to init everything
    if (s_mux_ok) mux_select(0); 

    s_adc_ok = i2c_probe(ADDR_ADS1115);
    ESP_LOGI(TAG, "ADS1115 %s", s_adc_ok ? "OK" : "NOT FOUND");

    return true;
}

void hal_restart(void) {
    esp_restart();
}