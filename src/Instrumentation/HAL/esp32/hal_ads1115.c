/**
 * @file hal_ads1115.c
 * @brief ADS1115 4-channel ADC (I2C addr 0x48).
 *
 * @note This used to probe fine and set its CAN status-OK bit while the
 *       actual channel readings were computed and then thrown away -
 *       control.c only ever checked hal_read_ads1115().ok to set a
 *       status bit, nothing looked at .mv[]. Logging the values here
 *       makes them visible again. If you want them on CAN too, that
 *       needs a new CAN_MSG_* type in can_instr.h (Photonics' ADC
 *       message is a ready-made template) - ask if you want that added.
 */
#include "hal.h"
#include "hal_internal.h"

#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "instr_hal_ads1115";

#define ADDR_ADS1115 0x48

static bool s_ok;

bool hal_ads1115_probe(void) {
    s_ok = i2c_probe(ADDR_ADS1115);
    return s_ok;
}

hal_ads1115_t hal_read_ads1115(void) {
    hal_ads1115_t r = { 0 };
    if (!s_ok)
        return r;
    r.ok = true;

    for (int ch = 0; ch < 4; ch++) {
        uint16_t mux = (uint16_t)(0x4 + ch); /* single-ended AIN[ch] vs GND */
        uint16_t config = (uint16_t)((1u << 15)      /* start conversion */
            | (mux << 12) | (0x1u << 9)              /* +-4.096V FSR */
            | (1u << 8)                              /* single-shot */
            | (0x4u << 5)                            /* 1600 SPS */
            | 0x3u);                                 /* disable comparator */
        uint8_t wr[3] = { 0x01, (uint8_t)(config >> 8), (uint8_t)(config & 0xFF) };
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
        r.mv[ch] = (int16_t)lroundf(raw * 0.125f); /* LSB = 125uV at +-4.096V FSR */
    }

    if (r.ok) {
        ESP_LOGI(TAG, "ch0=%dmV ch1=%dmV ch2=%dmV ch3=%dmV", r.mv[0], r.mv[1], r.mv[2], r.mv[3]);
    }
    return r;
}