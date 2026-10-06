/**
 * @file hal_scd41.c
 * @brief SCD41 CO2/temperature/humidity sensor (I2C addr 0x62).
 */
#include "hal.h"
#include "hal_internal.h"

#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "instr_hal_scd41";

#define ADDR_SCD41 0x62

static bool s_ok;

static bool scd41_send_cmd(uint16_t cmd) {
    uint8_t wr[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_write(ADDR_SCD41, wr, sizeof wr);
}

bool hal_scd41_probe(void) {
    s_ok = i2c_probe(ADDR_SCD41);
    if (!s_ok)
        return false;

    scd41_send_cmd(0x3F86); /* stop_periodic_measurement, in case it was left running */
    vTaskDelay(pdMS_TO_TICKS(500));
    scd41_send_cmd(0x3646); /* reinit */
    vTaskDelay(pdMS_TO_TICKS(30));
    s_ok = scd41_send_cmd(0x21B1); /* start_periodic_measurement */
    return s_ok;
}

hal_scd41_t hal_read_scd41(void) {
    hal_scd41_t r = { 0 };
    if (!s_ok)
        return r;

    if (!scd41_send_cmd(0xE4B8))
        return r; /* get_data_ready_status */
    vTaskDelay(pdMS_TO_TICKS(2));
    uint8_t rdy[3];
    if (!i2c_read(ADDR_SCD41, rdy, sizeof rdy))
        return r;
    uint16_t status = ((uint16_t)rdy[0] << 8) | rdy[1];
    if ((status & 0x07FFu) == 0)
        return r; /* not ready yet () */

    if (!scd41_send_cmd(0xEC05))
        return r; /* read_measurement */
    vTaskDelay(pdMS_TO_TICKS(2));
    uint8_t buf[9];
    if (!i2c_read(ADDR_SCD41, buf, sizeof buf))
        return r;
    if (sensirion_crc8(&buf[0], 2) != buf[2] || sensirion_crc8(&buf[3], 2) != buf[5]
        || sensirion_crc8(&buf[6], 2) != buf[8]) {
        ESP_LOGW(TAG, "CRC mismatch");
        return r;
    }

    uint16_t co2 = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_t = ((uint16_t)buf[3] << 8) | buf[4];
    uint16_t raw_rh = ((uint16_t)buf[6] << 8) | buf[7];
    float temp_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    float rh_pct = 100.0f * ((float)raw_rh / 65535.0f);

    r.ok = true;
    r.co2_ppm = co2;
    r.temp_c_x10 = (int16_t)lroundf(temp_c * 10.0f);
    r.rh_pct_x10 = (int16_t)lroundf(rh_pct * 10.0f);
    return r;
}