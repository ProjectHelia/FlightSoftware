/* Photonics control core: services however many UV sensors and flask
 * ADS1115s the boot scan actually found (hal_num_uv()/hal_num_flasks() -
 * see hal.h for why that's discovered at boot rather than assumed) and
 * sends whatever's fresh onto CAN. Loop cadence (100ms) matches the
 * original bring-up test's loop() delay rather than Instrumentation's 1Hz
 * poll, since the LTR390 ping-pongs between ALS/UVS roughly every ~100ms
 * per mode and the ask here is maximum data, not a fixed telemetry rate.
 *
 * Guarded by ESP_PLATFORM so this compiles to nothing on the host, same
 * as every other node's control.c.
 */
#include "control.h"

#ifdef ESP_PLATFORM
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "hal.h"
#include "can_bus.h"
#include "can_photonics.h"
#include "util_timer.h"

static const char *TAG = "photonics_ctrl";

#define TX_TIMEOUT_MS 10u

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void send(const can_frame_t *f, const char *what)
{
    if (!can_bus_send(f, TX_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "%s send failed, id=0x%03lX", what, (unsigned long)f->id);
    }
}

void photonics_control_task(void *arg)
{
    (void)arg;
    uint32_t next_summary_ms = now_ms();
    uint32_t uv_frames[PHOTONICS_MAX_UV] = {0};
    uint32_t flask_frames[PHOTONICS_MAX_FLASKS] = {0};

    uint8_t num_uv = hal_num_uv();
    uint8_t num_flasks = hal_num_flasks();

    ESP_LOGI(TAG, "control core up - %u UV sensor(s), %u flask ADC(s)", num_uv, num_flasks);

    for (;;) {
        can_frame_t f;

        for (uint8_t i = 0; i < num_uv; i++) {
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

        for (uint8_t i = 0; i < num_flasks; i++) {
            hal_flask_t flask = hal_read_flask(i);
            if (flask.ok) {
                can_photonics_flask_t p;
                memcpy(p.mv, flask.mv, sizeof p.mv);
                can_encode_flask(i, &f, &p);
                char what[16];
                snprintf(what, sizeof what, "flask%u", i);
                send(&f, what);
                flask_frames[i]++;
            }
        }

        /* One line every 5s so you can see it's alive without needing a
         * fresh boot log - counts frames sent rather than instantaneous
         * values since sends happen faster than any one log line could
         * usefully show. */
        if (util_every(now_ms(), &next_summary_ms, 5000u)) {
            ESP_LOGI(TAG, "frames: uv0=%lu uv1=%lu flask0=%lu flask1=%lu flask2=%lu flask3=%lu",
                     (unsigned long)uv_frames[0], (unsigned long)uv_frames[1],
                     (unsigned long)flask_frames[0], (unsigned long)flask_frames[1],
                     (unsigned long)flask_frames[2], (unsigned long)flask_frames[3]);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); /* matches the original bring-up test's loop cadence */
    }
}
#endif /* ESP_PLATFORM */