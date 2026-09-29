#include "control.h"

#ifdef ESP_PLATFORM
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "hal.h"
#include "can_bus.h"
#include "can_photonics.h"
#include "util_timer.h"

static const char *TAG = "photonics_ctrl";

#define TX_TIMEOUT_MS 10u

// debugging variable
#define SEND_ADC 1

static uint32_t now_ms(void) {
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void send(const can_frame_t *f, const char *what) {
    if (!can_bus_send(f, TX_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "%s send failed, id=0x%03lX", what, (unsigned long)f->id);
    }
}

void photonics_control_task(void *arg) {

    (void)arg; // Just to avoid warnings again

    uint32_t next_summary_ms = now_ms();
    uint32_t uv_frames[PHOTONICS_NUM_UV] = { 0 };
    uint32_t adc_frames = 0;

    ESP_LOGI(TAG, "control core up");

    while (1) {
        can_frame_t f;

        for (uint8_t i = 0; i < PHOTONICS_NUM_UV; i++) {

            hal_uv_t uv = hal_read_uv(i);

            if (uv.ok && uv.fresh) {
                can_photonics_uv_t p = { .lux_x10 = uv.lux_x10, .uvi_x1000 = uv.uvi_x1000 };

                if (i == 0) {
                    can_encode_uv0(&f, &p);
                    send(&f, "uv0");
                } else {
                    can_encode_uv1(&f, &p);
                    send(&f, "uv1");
                }
                uv_frames[i]++;
            }
        }

#if SEND_ADC
        hal_adc_t adc = hal_read_adc();
        if (adc.ok) {
            can_photonics_adc_t a;
            memcpy(a.mv, adc.mv, sizeof a.mv);
            can_encode_adc(&f, &a);
            send(&f, "adc");
            adc_frames++;
        }
#endif
        // Log a summary every 5 seconds, useful for debugging, might remove later
        if (util_every(now_ms(), &next_summary_ms, 5000u)) {

            ESP_LOGI(TAG, "frames: uv0=%lu uv1=%lu adc=%lu",
                (unsigned long)uv_frames[0], (unsigned long)uv_frames[1],
                (unsigned long)adc_frames);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif /* ESP_PLATFORM */