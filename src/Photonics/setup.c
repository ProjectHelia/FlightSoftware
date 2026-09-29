#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "hal.h"
#include "can_bus.h"
#include "comms.h"
#include "control.h"

static const char *TAG = "photonics";

#define RX_TIMEOUT_MS 20u
#define TX_TIMEOUT_MS 10u

static uint32_t now_ms(void) {
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void comms_task(void *arg) {
    (void)arg; // unsused, just added this here to avoid compiler warnings :)

    photonics_comms_t comms;
    uint32_t last_status_log_ms = 0;

    photonics_comms_init(&comms, now_ms());
    photonics_comms_boot_done(&comms);

    ESP_LOGI(TAG, "state %s", fsm_state_name(comms.state));

    while (1) {
        can_frame_t rx, tx[PHOTONICS_COMMS_MAX_TX];
        fsm_state_t before = comms.state;

        if (can_bus_recv(&rx, RX_TIMEOUT_MS))
            photonics_comms_on_frame(&comms, &rx);

        size_t n = photonics_comms_on_tick(&comms, now_ms(), tx);

        for (size_t i = 0; i < n; i++) {

            if (!can_bus_send(&tx[i], TX_TIMEOUT_MS)) {
                ESP_LOGW(TAG, "send failed, id=0x%03lX", (unsigned long)tx[i].id);

                if (now_ms() - last_status_log_ms >= 2000u) {
                    can_bus_log_status();
                    last_status_log_ms = now_ms();
                }
            }
        }

        if (comms.state != before)
            ESP_LOGI(TAG, "state %s -> %s", fsm_state_name(before), fsm_state_name(comms.state));
        if (comms.reset_requested)
            hal_restart();

        can_bus_poll_recovery();
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "boot");
    if (!hal_init())
        ESP_LOGE(TAG, "CAN init failed");

    xTaskCreatePinnedToCore(comms_task, "photonics_comms", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(photonics_control_task, "photonics_ctrl", 4096, NULL, 5, NULL, 1);
}