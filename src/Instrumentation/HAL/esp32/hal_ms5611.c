/**
 * @file hal_ms5611.c
 * @brief MS5611 barometric pressure/temperature sensor (I2C addr 0x76 or
 *        0x77, depending on how CSB is wired - both are tried at probe
 *        time).
 */
#include "hal.h"
#include "hal_internal.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "instr_hal_ms5611";

#define ADDR_MS5611_A 0x76
#define ADDR_MS5611_B 0x77

#define CMD_RESET 0x1E
#define CMD_CONVERT_D1_OSR4096 0x48 /* pressure */
#define CMD_CONVERT_D2_OSR4096 0x58 /* temperature */
#define CMD_ADC_READ 0x00
#define CONVERT_DELAY_MS 10 /* OSR4096 max conversion time is 9.04ms per datasheet */
#define RESET_SETTLE_MS 15 /* datasheet quotes ~2.8ms reset time - that's nominal, not
                             * worst-case; the first few PROM reads were failing at 3ms
                             * while later ones in the same loop succeeded, which is the
                             * signature of "not quite long enough", not a real fault. */

static bool s_ok;
static uint8_t s_addr;
static uint16_t s_c[6]; /* PROM calibration coefficients C1..C6 */

bool hal_ms5611_probe(void) {
    if (i2c_probe(ADDR_MS5611_A)) {
        s_addr = ADDR_MS5611_A;
    } else if (i2c_probe(ADDR_MS5611_B)) {
        s_addr = ADDR_MS5611_B;
    } else {
        ESP_LOGW(TAG, "not found at 0x76 or 0x77");
        s_ok = false;
        return false;
    }

    uint8_t reset = CMD_RESET;
    i2c_write(s_addr, &reset, 1);
    vTaskDelay(pdMS_TO_TICKS(RESET_SETTLE_MS));

    /* Tracks success across the WHOLE loop - do not reuse s_ok for this,
     * it only reflects whichever iteration ran last (that was the actual
     * bug: C1-C3 could fail, C6 succeeds, and s_ok came out true because
     * it was last written by C6's result). */
    bool prom_ok = true;
    for (int i = 0; i < 6; i++) {
        uint8_t cmd = (uint8_t)(0xA2 + i * 2); /* PROM addr 1..6 */
        uint8_t rd[2];
        if (!i2c_write_read(s_addr, &cmd, 1, rd, sizeof rd)) {
            ESP_LOGW(TAG, "PROM read failed at C%d", i + 1);
            prom_ok = false;
            continue; /* keep going so the log shows every failing word, not just the first */
        }
        s_c[i] = (uint16_t)(((uint16_t)rd[0] << 8) | rd[1]);
    }

    if (!prom_ok) {
        ESP_LOGW(TAG, "probe failed - PROM read incomplete, addr=0x%02X", s_addr);
        s_ok = false;
        return false;
    }

    ESP_LOGI(TAG, "addr=0x%02X C1..C6=%04X %04X %04X %04X %04X %04X", s_addr, s_c[0], s_c[1],
        s_c[2], s_c[3], s_c[4], s_c[5]);

    s_ok = true;
    return true;
}

uint8_t hal_ms5611_addr(void) {
    return s_addr;
}

static bool ms5611_read_adc(uint8_t convert_cmd, uint32_t *out) {
    if (!i2c_write(s_addr, &convert_cmd, 1))
        return false;
    vTaskDelay(pdMS_TO_TICKS(CONVERT_DELAY_MS));

    uint8_t cmd = CMD_ADC_READ, rd[3];
    if (!i2c_write_read(s_addr, &cmd, 1, rd, sizeof rd))
        return false;

    *out = ((uint32_t)rd[0] << 16) | ((uint32_t)rd[1] << 8) | rd[2];
    return true;
}

hal_ms5611_t hal_read_ms5611(void) {
    hal_ms5611_t r = { 0 };
    if (!s_ok)
        return r;

    uint32_t d1, d2;
    if (!ms5611_read_adc(CMD_CONVERT_D1_OSR4096, &d1)) {
        ESP_LOGW(TAG, "D1 (pressure) conversion read failed");
        return r;
    }
    if (!ms5611_read_adc(CMD_CONVERT_D2_OSR4096, &d2)) {
        ESP_LOGW(TAG, "D2 (temperature) conversion read failed");
        return r;
    }

    /* Standard MS5611 second-order compensation, straight from the
     * datasheet (C1..C6 = s_c[0..5]). Needs 64-bit intermediates - SENS
     * and OFF both overflow 32 bits partway through. */
    int64_t dT = (int64_t)d2 - ((int64_t)s_c[4] << 8);
    int64_t temp = 2000 + ((dT * s_c[5]) >> 23);          /* 0.01 degC units */
    int64_t off = ((int64_t)s_c[1] << 16) + ((s_c[3] * dT) >> 7);
    int64_t sens = ((int64_t)s_c[0] << 15) + ((s_c[2] * dT) >> 8);
    int64_t pressure = (((int64_t)d1 * sens) >> 21) - off;
    pressure >>= 15; /* 0.01 mbar units */

    r.ok = true;
    r.pressure_mbar_x10 = (int32_t)(pressure / 10);
    r.temp_c_x10 = (int16_t)(temp / 10);
    return r;
}