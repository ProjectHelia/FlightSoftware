#ifndef HELIA_ETH_W5500_H
#define HELIA_ETH_W5500_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/* Bring up SPI + W5500 + esp_netif and start the driver. Non-blocking:
 * link/IP arrive later via events. */
esp_err_t ttc_eth_init(void);

/* True once the cable is up AND the interface has an IP. */
bool ttc_eth_ready(void);

/* Block until ready or timeout. Returns ttc_eth_ready(). */
bool ttc_eth_wait_ready(TickType_t timeout);

#endif