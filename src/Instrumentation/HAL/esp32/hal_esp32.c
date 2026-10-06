/**
 * @file hal_esp32.c
 * @brief Instrumentation ESP32 HAL: I2C bus setup + boot-time sensor
 *        probe sequence, plus hal_init()/hal_restart(). Each sensor's
 *        own read implementation lives in its own file next to this one
 *        (hal_sht45.c, hal_ads1115.c, hal_scd41.c, hal_ms5611.c,
 *        hal_imu.c) - this file only owns the shared I2C bus and the
 *        boot-time probe/scan sequence; see hal_internal.h for the
 *        shared plumbing between them.
 *
 * Uses the LEGACY driver/i2c.h API, not the newer driver/i2c_master.h -
 * switched after the new-driver version failed to probe anything at all
 * (100% timeout across every address) on hardware that's proven working
 * via Arduino's Wire library, which itself sits on top of this same
 * legacy driver family. Still fully native ESP-IDF, just the older API
 * surface.
 */
#include "hal.h"
#include "hal_internal.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#include "can_bus.h"
#include "pins.h"

static const char *TAG = "instr_hal";

#define SDA_GPIO 8
#define SCL_GPIO 9
#define I2C_HZ 100000

bool hal_init(void) {
    if (!can_bus_init(PIN_CAN_TX, PIN_CAN_RX))
        return false;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE, /* breakouts have their own external pull-ups */
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_HZ,
    };
    if (i2c_param_config(I2C_PORT, &conf) != ESP_OK
        || i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed - sensors unavailable this boot");
        return true; /* CAN is up, which is what's fatal */
    }

    /* SCD41 needs >=1s after power-up before it will ACK on the bus at all
     * (Sensirion datasheet). Everything else responds immediately, but this
     * delay is cheap and shared across all sensors, so pay it once here
     * rather than special-casing SCD41's probe. */
    vTaskDelay(pdMS_TO_TICKS(1000));

    int found = 0;
    ESP_LOGI(TAG, "I2C scan:");
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_probe(addr)) {
            ESP_LOGI(TAG, "  found 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG,
            "  nothing found - check wiring/pull-ups/SDA=%d,SCL=%d before trusting the "
            "per-sensor results below",
            SDA_GPIO, SCL_GPIO);
    }

    ESP_LOGW(TAG, "found %d I2C device%s", found, found == 1 ? "" : "s");

    bool ok_sht45 = hal_sht45_probe();
    bool ok_ads1115 = hal_ads1115_probe();
    bool ok_imu = hal_imu_probe();
    bool ok_scd41 = hal_scd41_probe();
    bool ok_ms5611 = hal_ms5611_probe();

    ESP_LOGI(TAG, "SHT45   %s", ok_sht45 ? "OK" : "NOT FOUND");
    ESP_LOGI(TAG, "ADS1115 %s", ok_ads1115 ? "OK" : "NOT FOUND");
    ESP_LOGI(TAG, "SCD41   %s", ok_scd41 ? "OK" : "NOT FOUND");
    if (ok_ms5611) {
        ESP_LOGI(TAG, "MS5611  OK (0x%02X)", hal_ms5611_addr());
    } else {
        ESP_LOGI(TAG, "MS5611  NOT FOUND");
    }
    ESP_LOGI(TAG, "FSP201  %s", ok_imu ? "OK" : "NOT FOUND");

    return true;
}

void hal_restart(void) {
    esp_restart();
}