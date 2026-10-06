/**
 * @file hal_sht45.c
 * @brief SHT45 temperature/humidity sensor (I2C addr 0x44).
 */
#include "hal.h"
#include "hal_internal.h"

#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "instr_hal_sht45";

#define ADDR_SHT45 0x44

static bool s_ok;

bool hal_sht45_probe(void) {
    s_ok = i2c_probe(ADDR_SHT45);
    return s_ok;
}

hal_sht45_t hal_read_sht45(void) {
    hal_sht45_t r = { 0 };
    if (!s_ok)
        return r;

    uint8_t cmd = 0xFD; /* measure, high repeatability */
    if (!i2c_write(ADDR_SHT45, &cmd, 1))
        return r;
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t buf[6];
    if (!i2c_read(ADDR_SHT45, buf, sizeof buf))
        return r;
    if (sensirion_crc8(&buf[0], 2) != buf[2] || sensirion_crc8(&buf[3], 2) != buf[5]) {
        ESP_LOGW(TAG, "CRC mismatch");
        return r;
    }

    uint16_t raw_t = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_rh = ((uint16_t)buf[3] << 8) | buf[4];
    float temp_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    float rh_pct = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);
    if (rh_pct < 0.0f)
        rh_pct = 0.0f;
    if (rh_pct > 100.0f)
        rh_pct = 100.0f;

    r.ok = true;
    r.temp_c_x10 = (int16_t)lroundf(temp_c * 10.0f);
    r.rh_pct_x10 = (int16_t)lroundf(rh_pct * 10.0f);
    return r;
}