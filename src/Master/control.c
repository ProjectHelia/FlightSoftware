/** @brief Master control core: owns the flight-phase state machine (flight_phase.h
 * does the actual FSM logic; this file just synchronises access to one
 * instance across cores and hooks it up to CAN + logging).
 *
 * Two callers touch the flight state from two different cores:
 *   - master_control_task (this file, core 1): periodic no-motion tick
 *   - master_control_set_phase (called from ttc.c's uplink handler, core
 *     0): ground-commanded phase changes
 * Hence the mutex - see master_control_init()'s doc comment in control.h
 * for why it's created eagerly from app_main() rather than lazily here.
 */
#include "control.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "can_bus.h"
#include "protocol.h"

static const char *TAG = "master_ctrl";

#define TX_TIMEOUT_MS 10u

static master_flight_t s_flight;
static SemaphoreHandle_t s_flight_mutex;

static uint32_t now_ms(void) {
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

/** @brief Tells every other node the phase changed */
static void broadcast_phase(master_phase_t phase, master_flight_result_t result) {
    can_frame_t f = {
        .id = can_make_id(CAN_PRIO_IMPORTANT, CAN_SRC_BROADCAST, CAN_CMD_FLIGHT_PHASE),
        .dlc = 1,
    };
    f.data[0] = (uint8_t)phase;

    if (!can_bus_send(&f, TX_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "failed to broadcast phase change to CAN");
    }

    if (result == MASTER_FLIGHT_OVERRIDE) {
        ESP_LOGW(TAG, "flight phase -> %s (OUT OF SEQUENCE - ground override)", master_phase_name(phase));
    } else {
        ESP_LOGI(TAG, "flight phase -> %s", master_phase_name(phase));
    }
}

void master_control_init(void) {
    s_flight_mutex = xSemaphoreCreateMutex();
    master_flight_init(&s_flight, now_ms());
}

master_flight_result_t master_control_set_phase(master_phase_t next) {
    xSemaphoreTake(s_flight_mutex, portMAX_DELAY);
    master_flight_result_t result = master_flight_set_phase(&s_flight, next, now_ms());
    if (result != MASTER_FLIGHT_REJECTED) {
        broadcast_phase(next, result);
    }
    xSemaphoreGive(s_flight_mutex);
    return result;
}

master_phase_t master_control_get_phase(void) {
    xSemaphoreTake(s_flight_mutex, portMAX_DELAY);
    master_phase_t p = master_flight_phase(&s_flight);
    xSemaphoreGive(s_flight_mutex);
    return p;
}

void master_control_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "control core up, flight phase %s", master_phase_name(master_flight_phase(&s_flight)));

    for (;;) {
        xSemaphoreTake(s_flight_mutex, portMAX_DELAY);
        master_phase_t before = master_flight_phase(&s_flight);
        master_flight_tick(&s_flight, now_ms());
        master_phase_t after = master_flight_phase(&s_flight);
        if (after != before) {
            /* only the automatic DESCENDING -> LANDED path lands here */
            broadcast_phase(after, MASTER_FLIGHT_OK);
        }
        xSemaphoreGive(s_flight_mutex);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* ESP_PLATFORM */