/* W5500 Ethernet bring-up. This is a thin driver shim, same kind of thing
 * as can_bus_twai.c: it brings up a peripheral and reports link/IP state,
 * owns no task and no queue of its own. Lives in Master's HAL because only
 * Master has Ethernet - it isn't a reusable lib. */
#include "eth_w5500.h"

#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "eth";

#define BIT_LINK BIT0
#define BIT_IP BIT1

static EventGroupHandle_t s_eg;
static esp_netif_t *s_netif;

#if CONFIG_TTC_USE_STATIC_IP
static void apply_static_ip(void) {
    esp_err_t err = esp_netif_dhcpc_stop(s_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "dhcpc_stop failed: %s", esp_err_to_name(err));
        return;
    }

    esp_netif_ip_info_t ip = { 0 };
    ip.ip.addr = ipaddr_addr(CONFIG_TTC_STATIC_IP);
    ip.netmask.addr = ipaddr_addr(CONFIG_TTC_STATIC_NETMASK);
    ip.gw.addr = ipaddr_addr(CONFIG_TTC_STATIC_GW);

    err = esp_netif_set_ip_info(s_netif, &ip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "static IP " IPSTR " / " IPSTR, IP2STR(&ip.ip), IP2STR(&ip.netmask));
    xEventGroupSetBits(s_eg, BIT_IP);
}
#endif

static void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    esp_eth_handle_t h = *(esp_eth_handle_t *)data;
    uint8_t mac[6] = { 0 };

    switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(h, ETH_CMD_G_MAC_ADDR, mac);
            ESP_LOGI(TAG, "link UP  mac %02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            xEventGroupSetBits(s_eg, BIT_LINK);
#if CONFIG_TTC_USE_STATIC_IP
            apply_static_ip();
#endif
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "link DOWN");
            xEventGroupClearBits(s_eg, BIT_LINK | BIT_IP);
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "driver started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGW(TAG, "driver stopped");
            break;
        default:
            break;
    }
}

static void got_ip_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
    xEventGroupSetBits(s_eg, BIT_IP);
}

esp_err_t ttc_eth_init(void) {
    s_eg = xEventGroupCreate();

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_handler, NULL));

    /* ISR service is needed for the W5500 INT line */
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    spi_bus_config_t bus = {
        .miso_io_num = CONFIG_TTC_ETH_MISO_GPIO,
        .mosi_io_num = CONFIG_TTC_ETH_MOSI_GPIO,
        .sclk_io_num = CONFIG_TTC_ETH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .mode = 0,
        .clock_speed_hz = CONFIG_TTC_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = CONFIG_TTC_ETH_CS_GPIO,
    };

    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &dev);
    w5500_cfg.int_gpio_num = CONFIG_TTC_ETH_INT_GPIO;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    if (CONFIG_TTC_ETH_INT_GPIO < 0) {
        w5500_cfg.poll_period_ms = CONFIG_TTC_ETH_POLL_PERIOD_MS;
    }
#endif

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = CONFIG_TTC_ETH_RST_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "failed to create W5500 mac/phy");
        return ESP_FAIL;
    }

    static esp_eth_handle_t handle; /* static: referenced by event data */
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_cfg, &handle);
    if (err != ESP_OK) {
        /* Most common cause on bring-up: wrong SPI pins / no power to W5500 */
        ESP_LOGE(TAG, "driver install failed (%s) - check SPI wiring and pins",
            esp_err_to_name(err));
        return err;
    }

    /* W5500 has no factory MAC; use the ESP32's eFuse-derived Ethernet MAC */
    uint8_t mac_addr[6];
    ESP_ERROR_CHECK(esp_read_mac(mac_addr, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    ESP_ERROR_CHECK(esp_netif_attach(s_netif, esp_eth_new_netif_glue(handle)));
    ESP_ERROR_CHECK(esp_eth_start(handle));

    ESP_LOGI(TAG, "W5500 up: SCLK=%d MOSI=%d MISO=%d CS=%d INT=%d RST=%d @ %d MHz",
        CONFIG_TTC_ETH_SCLK_GPIO, CONFIG_TTC_ETH_MOSI_GPIO, CONFIG_TTC_ETH_MISO_GPIO,
        CONFIG_TTC_ETH_CS_GPIO, CONFIG_TTC_ETH_INT_GPIO, CONFIG_TTC_ETH_RST_GPIO,
        CONFIG_TTC_ETH_SPI_CLOCK_MHZ);
    return ESP_OK;
}

bool ttc_eth_ready(void) {
    const EventBits_t need = BIT_LINK | BIT_IP;
    return (xEventGroupGetBits(s_eg) & need) == need;
}

bool ttc_eth_wait_ready(TickType_t timeout) {
    const EventBits_t need = BIT_LINK | BIT_IP;
    EventBits_t b = xEventGroupWaitBits(s_eg, need, pdFALSE, pdTRUE, timeout);
    return (b & need) == need;
}