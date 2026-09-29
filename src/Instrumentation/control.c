#include "control.h"

#ifdef ESP_PLATFORM
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "hal.h"
#include "can_bus.h"
#include "can_instr.h"
#include "util_timer.h"

static const char *TAG = "instr_ctrl";

#define TX_TIMEOUT_MS 10u
#define POLL_PERIOD_MS 1000u 

/* TODO(remy): update this to be an accurate atmospheric reference */
#define SEA_LEVEL_MBAR 1013.25f

/*
 * This entire file needs debugging and testing, some sensors are missing from the original set up and can be removed too.
 * TODO(remy): do that
*/

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void send(const can_frame_t *f, const char *what)
{
    if (!can_bus_send(f, TX_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "%s send failed, id=0x%03lX", what, (unsigned long)f->id);
    }
}

void instr_control_task(void *arg)
{
    (void)arg;

    uint32_t next_poll_ms = now_ms();
    uint32_t next_summary_ms = now_ms();

    ESP_LOGI(TAG, "control core up");

    for (;;) {
        if (util_every(now_ms(), &next_poll_ms, POLL_PERIOD_MS)) {
            can_frame_t f;
            bool sent_baro = false, sent_sht = false, sent_gas = false, sent_imu = false;
            uint8_t status = 0;

            hal_ms5611_t baro = hal_read_ms5611();
            int16_t board_temp_c_x10 = 0;
            if (baro.ok) {
                sent_baro = true;
                status |= CAN_INSTR_STATUS_BARO;
                board_temp_c_x10 = baro.temp_c_x10; 
                float pressure_mbar = baro.pressure_mbar_x10 / 10.0f;
                float altitude_m = 44330.0f * (1.0f - powf(pressure_mbar / SEA_LEVEL_MBAR, 1.0f / 5.255f));

                can_instr_housekeeping_t hk = {
                    .pressure_mbar_x10 = baro.pressure_mbar_x10,
                    .altitude_m_x10    = (int32_t)lroundf(altitude_m * 10.0f),
                };
                can_encode_housekeeping(&f, &hk);
                send(&f, "housekeeping");
            }

            hal_sht45_t sht = hal_read_sht45();
            if (sht.ok) {
                sent_sht = true;
                status |= CAN_INSTR_STATUS_SHT;
                can_instr_humidity_t h = { .temp_c_x10 = sht.temp_c_x10, .rh_pct_x10 = sht.rh_pct_x10 };
                can_encode_humidity(&f, &h);
                send(&f, "humidity");
            }

            hal_scd41_t gas = hal_read_scd41();
            if (gas.ok) {
                sent_gas = true;
                status |= CAN_INSTR_STATUS_GAS;
                can_instr_gas_t g = { .co2_ppm = gas.co2_ppm, .temp_c_x10 = gas.temp_c_x10, .rh_pct_x10 = gas.rh_pct_x10 };
                can_encode_gas(&f, &g);
                send(&f, "gas");
            }

            hal_imu_t imu = hal_read_imu();
            if (imu.ok) {
                sent_imu = true;
                status |= CAN_INSTR_STATUS_IMU;
                can_instr_accel_t a = { .ax_mmss = imu.ax_mmss, .ay_mmss = imu.ay_mmss, .az_mmss = imu.az_mmss };
                can_encode_accel(&f, &a);
                send(&f, "accel");
            }
            hal_ads1115_t ads = hal_read_ads1115();
            if (ads.ok) status |= CAN_INSTR_STATUS_ADS;

            can_instr_status_t st = { .sensor_status = status, .board_temp_c_x10 = board_temp_c_x10 };
            can_encode_status(&f, &st);
            send(&f, "status");

            if (util_every(now_ms(), &next_summary_ms, 5000u)) {
                
                // Log a summary every 5 seconds, useful for debugging, might remove later
                ESP_LOGI(TAG, "sensors: baro=%s sht=%s gas=%s imu=%s ads=%s",
                         sent_baro ? "ok" : "-", sent_sht ? "ok" : "-",
                         sent_gas  ? "ok" : "-", sent_imu ? "ok" : "-",
                         (status & CAN_INSTR_STATUS_ADS) ? "ok" : "-");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif /* ESP_PLATFORM */
