#include "can_bus.h"
#include <string.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "can_bus";

#ifdef HELIA_CAN_SELF_TEST
#define TWAI_MODE TWAI_MODE_NO_ACK
#else
#define TWAI_MODE TWAI_MODE_NORMAL
#endif

#define CAN_RX_QUEUE_LEN 64u
#define CAN_TX_QUEUE_LEN 16u

#define CAN_ALERTS (TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_BUS_OFF | \
                    TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_ERR_PASS)

static uint32_t s_rx_dropped; /* frames lost to RX_QUEUE_FULL since boot */

static void check_rx_line(int rx_gpio)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << rx_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    if (gpio_config(&cfg) != ESP_OK) return;

    int high = 0;
    const int samples = 200; /* 200 x 50 us = 10 ms, ~5 frame times at 500 kbit/s */
    for (int i = 0; i < samples; i++) {
        high += gpio_get_level((gpio_num_t)rx_gpio);
        esp_rom_delay_us(50);
    }

    if (high == 0) {
        ESP_LOGE(TAG, "RX (GPIO%d) stuck LOW: bus held dominant. Check the transceiver "
                      "(powered? TX/RX swapped?) and CANH/CANL for a short.", rx_gpio);
    } else if (high == samples) {
        ESP_LOGI(TAG, "bus idle before start (no traffic in 10 ms)");
    } else {
        ESP_LOGI(TAG, "bus traffic seen before start (%d%% idle)", high * 100 / samples);
    }
}

bool can_bus_init(int tx_gpio, int rx_gpio)
{
    check_rx_line(rx_gpio);

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)tx_gpio, (gpio_num_t)rx_gpio, TWAI_MODE);
    g.rx_queue_len = CAN_RX_QUEUE_LEN;
    g.tx_queue_len = CAN_TX_QUEUE_LEN;
    g.alerts_enabled = CAN_ALERTS;

    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); /* CAN_BITRATE */
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g, &t, &f) != ESP_OK || twai_start() != ESP_OK) {
        ESP_LOGE(TAG, "TWAI start failed (TX=%d RX=%d)", tx_gpio, rx_gpio);
        return false;
    }
    s_rx_dropped = 0;
    ESP_LOGI(TAG, "CAN up, TX=%d RX=%d, rx_queue=%u%s", tx_gpio, rx_gpio, CAN_RX_QUEUE_LEN,
             TWAI_MODE == TWAI_MODE_NO_ACK ? " (SELF-TEST, no ACK needed)" : "");
    return true;
}

bool can_bus_send(const can_frame_t *f, uint32_t timeout_ms)
{
    twai_message_t m = {0};
    m.identifier       = f->id;
    m.data_length_code = f->dlc;
    memcpy(m.data, f->data, f->dlc);
#ifdef HELIA_CAN_SELF_TEST
    m.self = 1; /* receive our own frame back */
#endif
    return twai_transmit(&m, pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
}

bool can_bus_recv(can_frame_t *f, uint32_t timeout_ms)
{
    twai_message_t m;
    if (twai_receive(&m, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) return false;
    if (m.extd || m.rtr) return false; /* HELIA only uses standard data frames */

    f->id  = m.identifier;
    f->dlc = (m.data_length_code > CAN_MAX_DLC) ? (uint8_t)CAN_MAX_DLC : m.data_length_code;
    memcpy(f->data, m.data, f->dlc);
    return true;
}

void can_bus_poll_recovery(void)
{
    uint32_t alerts;
    if (twai_read_alerts(&alerts, 0) == ESP_OK) {
        if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
            s_rx_dropped++;
            ESP_LOGW(TAG, "RX queue full, frame dropped (total %lu)", (unsigned long)s_rx_dropped);
        }
        if (alerts & TWAI_ALERT_ERR_PASS) {
            ESP_LOGW(TAG, "entered error-passive state");
        }
        if (alerts & TWAI_ALERT_BUS_RECOVERED) {
            ESP_LOGI(TAG, "bus recovered");
        }
    }

    twai_status_info_t s;
    if (twai_get_status_info(&s) != ESP_OK) return;

    if (s.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW(TAG, "bus-off, recovering");
        twai_initiate_recovery();
    } else if (s.state == TWAI_STATE_STOPPED) {
        ESP_LOGI(TAG, "recovered, restarting");
        twai_start();
    }
}

/** @brief Frames lost to RX_QUEUE_FULL since can_bus_init(). */
uint32_t can_bus_dropped_frames(void)
{
    return s_rx_dropped;
}

void can_bus_log_status(void)
{
    static const char *const NAMES[] = { "STOPPED", "RUNNING", "BUS_OFF", "RECOVERING" };
    twai_status_info_t s;
    if (twai_get_status_info(&s) != ESP_OK) {
        ESP_LOGW(TAG, "driver not installed");
        return;
    }
    ESP_LOGW(TAG, "state=%s tx_err=%lu rx_err=%lu waiting_to_send=%lu bus_errors=%lu arb_lost=%lu received=%lu rx_dropped=%lu",
             (unsigned)s.state < 4 ? NAMES[s.state] : "?",
             (unsigned long)s.tx_error_counter, (unsigned long)s.rx_error_counter,
             (unsigned long)s.msgs_to_tx, (unsigned long)s.bus_error_count,
             (unsigned long)s.arb_lost_count, (unsigned long)s.msgs_to_rx,
             (unsigned long)s_rx_dropped);
}