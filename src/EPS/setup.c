/* EPS boot and task loops. The only EPS file that touches FreeRTOS: all the
 * logic lives in control.c and comms.c, where it can be unit-tested. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "hal.h"
#include "can_bus.h"
#include "control.h"
#include "comms.h"

static const char *TAG = "eps";

#define CONTROL_PERIOD_MS 100u /* 10 Hz sampling */
#define RX_TIMEOUT_MS     20u  /* how long comms waits for a frame each loop */
#define TX_TIMEOUT_MS     10u

/* Length-1 queue used as a mailbox: control overwrites, comms peeks the latest. */
static QueueHandle_t s_latest;

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}
static void control_task(void *arg)
{
    (void)arg;
    eps_control_t ctl;
    eps_control_init(&ctl);
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        eps_raw_t raw;
        eps_data_t data;

        raw.shunt_ok = hal_get_shunt_mv(&raw.shunt_mv);
        raw.therm_ok = hal_get_thermistor_mv(&raw.therm_mv);
        eps_control_step(&ctl, &raw, &data);
        xQueueOverwrite(s_latest, &data);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

static void comms_task(void *arg)
{
    (void)arg;
    eps_comms_t comms;
    uint32_t last_status_log_ms = 0;
    eps_comms_init(&comms, now_ms());
    eps_comms_boot_done(&comms); /* INIT -> SAFE */
    ESP_LOGI(TAG, "state %s", fsm_state_name(comms.state));

    for (;;) {
        eps_data_t latest;
        can_frame_t rx, tx[EPS_COMMS_MAX_TX];
        fsm_state_t before = comms.state;

        if (can_bus_recv(&rx, RX_TIMEOUT_MS)) {
            ESP_LOGI(TAG, "rx id=0x%03lX dlc=%u", (unsigned long)rx.id, rx.dlc); /* bench debug: remove once the bus works */
            eps_comms_on_frame(&comms, &rx);
        }

        if (xQueuePeek(s_latest, &latest, 0) != pdTRUE) eps_data_init(&latest);
        size_t n = eps_comms_on_tick(&comms, now_ms(), &latest, tx);
        for (size_t i = 0; i < n; i++) {
            if (!can_bus_send(&tx[i], TX_TIMEOUT_MS)) {
                ESP_LOGW(TAG, "send failed, id=0x%03lX", (unsigned long)tx[i].id);
                if (now_ms() - last_status_log_ms >= 2000u) {
                    can_bus_log_status();
                    last_status_log_ms = now_ms();
                }
            }
        }

        if (comms.state != before) ESP_LOGI(TAG, "state %s -> %s", fsm_state_name(before), fsm_state_name(comms.state));
        if (comms.reset_requested) hal_restart();
        can_bus_poll_recovery();
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "boot");
    if (!hal_init()) ESP_LOGE(TAG, "hardware init incomplete; affected readings will be sent as invalid");

    s_latest = xQueueCreate(1, sizeof(eps_data_t));

    xTaskCreatePinnedToCore(comms_task,   "eps_comms",   4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(control_task, "eps_control", 4096, NULL,  5, NULL, 1);
}