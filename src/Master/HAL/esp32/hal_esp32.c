/* Master HAL for the real board. */
#include "hal.h"
#include "pins.h"
#include "can_bus.h"
#include "eth_w5500.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "master_hal";

bool hal_init(void)
{
    if (!can_bus_init(PIN_CAN_TX, PIN_CAN_RX)) return false;

    /* esp_netif/esp_event are process-wide singletons; ESP_ERR_INVALID_STATE
     * just means something else already brought them up, which is fine. */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s - continuing without TT&C", esp_err_to_name(err));
        return true;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s - continuing without TT&C",
                 esp_err_to_name(err));
        return true;
    }

    if (ttc_eth_init() != ESP_OK) {
        ESP_LOGE(TAG, "TT&C Ethernet init failed - continuing without ground link");
    }
    return true;
}

void hal_restart(void)
{
    esp_restart();
}