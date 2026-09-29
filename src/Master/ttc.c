#include "ttc.h"

#include <string.h>

#define TTC_DL_HDR_LEN 4
#define TTC_DL_FRAME_LEN 11

size_t ttc_pack_downlink(uint8_t *pkt, size_t pkt_len, uint8_t seq, const can_frame_t *f) {
    if (pkt_len < TTC_DL_HDR_LEN + TTC_DL_FRAME_LEN)
        return 0;

    const uint8_t dlc = f->dlc > 8 ? 8 : f->dlc;
    pkt[0] = seq;
    pkt[1] = (uint8_t)can_id_priority(f->id);
    pkt[2] = 0;
    pkt[3] = TTC_DL_FRAME_LEN;
    pkt[4] = (uint8_t)(f->id >> 8); /* f->id is 11 bits: fits one byte plus 3 bits */
    pkt[5] = (uint8_t)(f->id & 0xFF);
    pkt[6] = dlc;
    memset(&pkt[7], 0, 8);
    memcpy(&pkt[7], f->data, dlc);
    return TTC_DL_HDR_LEN + TTC_DL_FRAME_LEN;
}

/* Everything below owns a task and sockets, so it's ESP-IDF-only
 * This is very close to what the old ttc_uplink_task() did, but now it's a separate file
 * I'm very close to putting this in the HAL layer (TODO?)
 */
#ifdef ESP_PLATFORM

#include <errno.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "eth_w5500.h"
#include "control.h"
#include "protocol.h"

static const char *TAG = "master_ttc";

#define RX_BUF_LEN 64

#define TTC_UL_SYNC 0xA5
#define TTC_UL_HDR_LEN 4
#define TTC_UL_MAX_PAYLOAD 8
#define TTC_ACK_HDR_LEN 5
#define TTC_UL_FLAG_TEST 0x80
#define TTC_TEST_PING 0x01
#define TTC_ACK_OK 0x00
#define TTC_ACK_BAD_LEN 0x01
#define TTC_ACK_BAD_PHASE 0x02

static bool send_all(int sock, const uint8_t *buf, size_t len) {
    while (len) {
        int n = send(sock, buf, len, 0);
        if (n <= 0)
            return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

static void send_ack(int sock, uint8_t cmd, uint8_t flags, uint8_t status,
    const uint8_t *payload, uint8_t len) {
    uint8_t ack[TTC_ACK_HDR_LEN + TTC_UL_MAX_PAYLOAD];
    if (len > TTC_UL_MAX_PAYLOAD)
        len = TTC_UL_MAX_PAYLOAD;
    ack[0] = TTC_UL_SYNC;
    ack[1] = cmd;
    ack[2] = flags;
    ack[3] = status;
    ack[4] = len;
    if (len)
        memcpy(&ack[TTC_ACK_HDR_LEN], payload, len);
    send_all(sock, ack, TTC_ACK_HDR_LEN + len);
}

/* No general CAN forwarding yet - see ttc.h
 * This is just a proof of concept for the EAR demo, full TT&C still need to be tested
 */
static void handle_command(int sock, uint8_t cmd, uint8_t flags, const uint8_t *p, uint8_t len) {
    if ((flags & TTC_UL_FLAG_TEST) && cmd == TTC_TEST_PING) {
        send_ack(sock, cmd, flags, TTC_ACK_OK, p, len);
        return;
    }

    if (cmd == CAN_CMD_FLIGHT_PHASE) {
        if (len < 1) {
            ESP_LOGW(TAG, "SET_FLIGHT_PHASE: missing phase byte");
            send_ack(sock, cmd, flags, TTC_ACK_BAD_LEN, NULL, 0);
            return;
        }
        master_phase_t requested = (master_phase_t)p[0];
        master_flight_result_t result = master_control_set_phase(requested);
        if (result == MASTER_FLIGHT_REJECTED) {
            ESP_LOGW(TAG, "SET_FLIGHT_PHASE: rejected phase byte 0x%02X", p[0]);
            send_ack(sock, cmd, flags, TTC_ACK_BAD_PHASE, NULL, 0);
        } else {
            send_ack(sock, cmd, flags, TTC_ACK_OK, p, len);
        }
        return;
    }

    ESP_LOGI(TAG, "uplink cmd 0x%02X flags 0x%02X len=%u (not forwarded to CAN yet)",
        cmd, flags, len);
    send_ack(sock, cmd, flags, TTC_ACK_OK, p, len);
}

/* Parse as many complete commands as the buffer holds; keep the remainder. */
static void process_stream(int sock, uint8_t *buf, size_t *fill) {
    size_t pos = 0;

    for (;;) {
        while (pos < *fill && buf[pos] != TTC_UL_SYNC)
            pos++;
        if (*fill - pos < TTC_UL_HDR_LEN)
            break;

        const uint8_t cmd = buf[pos + 1];
        const uint8_t flags = buf[pos + 2];
        const uint8_t len = buf[pos + 3];

        if (len > TTC_UL_MAX_PAYLOAD) {
            send_ack(sock, cmd, flags, TTC_ACK_BAD_LEN, NULL, 0);
            pos++; /* resync from next byte */
            continue;
        }
        if (*fill - pos < (size_t)TTC_UL_HDR_LEN + len)
            break; /* need more */

        handle_command(sock, cmd, flags, &buf[pos + TTC_UL_HDR_LEN], len);
        pos += TTC_UL_HDR_LEN + len;
    }

    memmove(buf, buf + pos, *fill - pos);
    *fill -= pos;
}

static void serve_client(int sock) {
    ESP_LOGI(TAG, "ground connected");
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    uint8_t buf[RX_BUF_LEN];
    size_t fill = 0;

    for (;;) {
        int n = recv(sock, buf + fill, sizeof(buf) - fill, 0);
        if (n == 0) {
            ESP_LOGW(TAG, "ground closed connection");
            break;
        }
        if (n < 0) {
            ESP_LOGW(TAG, "recv error: errno %d", errno);
            break;
        }
        fill += (size_t)n;
        process_stream(sock, buf, &fill);
        if (fill == sizeof(buf))
            fill = 0; /* can't happen with valid framing */
    }
}

void ttc_uplink_task(void *arg) {
    (void)arg;
    for (;;) {
        ttc_eth_wait_ready(portMAX_DELAY);

        int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (lsock < 0) {
            ESP_LOGE(TAG, "TCP socket failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        int one = 1;
        setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(CONFIG_TTC_TCP_UPLINK_PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY),
        };
        if (bind(lsock, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(lsock, 1) != 0) {
            ESP_LOGE(TAG, "bind/listen failed: errno %d", errno);
            close(lsock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "uplink listening on TCP %d", CONFIG_TTC_TCP_UPLINK_PORT);

        for (;;) {
            int csock = accept(lsock, NULL, NULL);
            if (csock < 0) {
                ESP_LOGW(TAG, "accept failed: errno %d", errno);
                break; /* rebuild listening socket */
            }
            serve_client(csock);
            shutdown(csock, SHUT_RDWR);
            close(csock);
        }
        close(lsock);
    }
}

#endif /* ESP_PLATFORM */