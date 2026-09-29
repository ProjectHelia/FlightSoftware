/* Master boot and task loop.
 *
 * Two cores, matching EPS/Instrumentation's comms/control split:
 *   core 0 (comms_task + ttc_uplink_task): CAN bus and the TT&C link
 *   core 1 (master_control_task): flight manager (will be extended in the future)
 *
 * TT&C is deliberately minimal right now, just enough to mirror CAN traffic down to ground for bench testing
 */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "sdkconfig.h"

#include "hal.h"
#include "can_bus.h"
#include "comms.h"
#include "control.h"
#include "eth_w5500.h"
#include "ttc.h"
#include "util_timer.h"

static const char *TAG = "master";

#define RX_TIMEOUT_MS 20u
#define TX_TIMEOUT_MS 10u

static uint32_t now_ms(void) {
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void send_all(const can_frame_t *tx, size_t n) {
    char text[96];
    for (size_t i = 0; i < n; i++) {
        if (!can_bus_send(&tx[i], TX_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "send failed, id=0x%03lX", (unsigned long)tx[i].id);
        } else if (can_id_type(tx[i].id) != CAN_MSG_HEARTBEAT || can_id_source(tx[i].id) != CAN_SRC_MASTER) {
            ESP_LOGI(TAG, "sent: %s", master_describe_frame(&tx[i], text, sizeof text)); /* don't log our own heartbeat ;-; */
        }
    }
}

/* "EPS online" / "EPS LOST" whenever a node appears or goes silent (good for debugging restarting) */
static void log_presence_changes(uint8_t before, uint8_t after) {
    for (unsigned i = 0; i < MASTER_NODE_SLOTS; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((after & bit) && !(before & bit))
            ESP_LOGI(TAG, "%s online", master_node_name((can_source_t)i));
        if ((before & bit) && !(after & bit))
            ESP_LOGW(TAG, "%s LOST (no heartbeat for %u ms)",
                master_node_name((can_source_t)i), (unsigned)CAN_HEARTBEAT_TIMEOUT_MS);
    }
}

/* Packs and mirrors one CAN frame down to ground over UDP. This will fail silently for now (TODO) */
static void downlink_frame(int sock, const struct sockaddr_in *ground, uint8_t *seq, const can_frame_t *f) {
    if (sock < 0 || !ttc_eth_ready())
        return;
    uint8_t pkt[TTC_DL_PACKET_LEN];
    size_t plen = ttc_pack_downlink(pkt, sizeof pkt, (*seq)++, f);
    if (plen)
        sendto(sock, pkt, plen, 0, (const struct sockaddr *)ground, sizeof *ground);
}

static void comms_task(void *arg) {
    (void)arg;
    master_comms_t comms;
    uint32_t last_status_log_ms = 0;
    master_comms_init(&comms, now_ms());
    ESP_LOGI(TAG, "state %s, auto-unlock %s", fsm_state_name(comms.state), MASTER_AUTO_UNLOCK ? "ON" : "OFF");

    /* One UDP socket, opened once, used to mirror CAN traffic down to
     * ground */
    int dl_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in ground = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_TTC_UDP_DOWNLINK_PORT),
        .sin_addr.s_addr = inet_addr(CONFIG_TTC_GROUND_IP),
    };
    uint8_t dl_seq = 0; 

    while (1) {
        can_frame_t rx, tx[MASTER_COMMS_MAX_TX];
        char text[96];
        uint8_t alive_before = comms.alive_mask;

        if (can_bus_recv(&rx, RX_TIMEOUT_MS)) {
            ESP_LOGI(TAG, "rx: %s", master_describe_frame(&rx, text, sizeof text));

            downlink_frame(dl_sock, &ground, &dl_seq, &rx);
            send_all(tx, master_comms_on_frame(&comms, now_ms(), &rx, tx));
        }

        size_t n = master_comms_on_tick(&comms, now_ms(), tx);
        send_all(tx, n);
        for (size_t i = 0; i < n; i++) {
            downlink_frame(dl_sock, &ground, &dl_seq, &tx[i]); /* was tx[0] only before - now mirrors all of them */
        }

        log_presence_changes(alive_before, comms.alive_mask);

        if (now_ms() - last_status_log_ms >= 5000u) { /* bus health every 5 s */
            can_bus_log_status();
            last_status_log_ms = now_ms();
        }
        can_bus_poll_recovery();
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "boot");
    if (!hal_init())
        ESP_LOGE(TAG, "CAN init failed");

    /* Must run before ttc_uplink_task is created */
    master_control_init();

    xTaskCreatePinnedToCore(comms_task, "master_comms", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(ttc_uplink_task, "master_ttc_ul", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(master_control_task, "master_ctrl", 4096, NULL, 5, NULL, 1);
}