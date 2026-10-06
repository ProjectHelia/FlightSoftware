/* Instrumentation control core: polls the five I2C sensors (via hal.h) and
 * sends their readings onto CAN (via can_instr.h). Any sensor reporting
 * !ok this tick (missing at boot, or SCD41 not ready yet) is just skipped -
 * same "missing sensor is skipped, not fatal" philosophy as the original
 * bring-up test.
 *
 * Guarded by ESP_PLATFORM so this compiles to nothing on the host, same as
 * every other node's control.c (the pure logic here is trivial enough -
 * mostly HAL calls and CAN sends - that there isn't a separate pure module
 * to split out, unlike Master's flight_phase.c).
 */
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
#define POLL_PERIOD_MS 1000u /* matches HOUSEKEEPING_DATA's 1 Hz in the SED */
#define SUMMARY_PERIOD_MS 5000u

/* How long a sensor can go without a good reading before it's actually
 * worth logging. Can't just be "not ok this exact tick" - SCD41 only has
 * fresh data ready roughly every 5s by its own internal timing, and the
 * IMU occasionally skips a report too, so a 1-tick snapshot would flag
 * both as "FAIL" on a majority of polls even when they're perfectly
 * healthy. 10s rides out that normal cadence while still catching a
 * sensor that's genuinely stopped responding. */
#define SENSOR_STALE_MS 10000u

/* Standard-atmosphere reference, not adjusted for local conditions/day-of
 * -flight QNH - fine for relative altitude trend, not absolute accuracy. */
#define SEA_LEVEL_MBAR 1013.25f

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

    /* Last time each sensor actually produced a good reading. Used instead
     * of a per-tick ok flag so the summary below reports real, sustained
     * failures rather than SCD41's normal "not ready this exact tick"
     * cadence (see SENSOR_STALE_MS above). */
    uint32_t last_ok_baro_ms = 0, last_ok_sht_ms = 0, last_ok_gas_ms = 0,
             last_ok_imu_ms = 0, last_ok_ads_ms = 0;

    ESP_LOGI(TAG, "control core up");

    for (;;) {
        if (util_every(now_ms(), &next_poll_ms, POLL_PERIOD_MS)) {
            can_frame_t f;
            uint8_t status = 0;
            uint32_t t = now_ms();

            hal_ms5611_t baro = hal_read_ms5611();
            int16_t board_temp_c_x10 = 0;
            if (baro.ok) {
                last_ok_baro_ms = t;
                status |= CAN_INSTR_STATUS_BARO;
                board_temp_c_x10 = baro.temp_c_x10; /* MS5611's own temp - was read every
                                                       * cycle and discarded before; now
                                                       * goes out in STATUS_DATA below. */
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
                last_ok_sht_ms = t;
                status |= CAN_INSTR_STATUS_SHT;
                can_instr_humidity_t h = { .temp_c_x10 = sht.temp_c_x10, .rh_pct_x10 = sht.rh_pct_x10 };
                can_encode_humidity(&f, &h);
                send(&f, "humidity");
            }

            hal_scd41_t gas = hal_read_scd41();
            if (gas.ok) {
                last_ok_gas_ms = t;
                status |= CAN_INSTR_STATUS_GAS;
                can_instr_gas_t g = { .co2_ppm = gas.co2_ppm, .temp_c_x10 = gas.temp_c_x10, .rh_pct_x10 = gas.rh_pct_x10 };
                can_encode_gas(&f, &g);
                send(&f, "gas");
            }

            hal_imu_t imu = hal_read_imu();
            if (imu.ok) {
                last_ok_imu_ms = t;
                status |= CAN_INSTR_STATUS_IMU;
                can_instr_accel_t a = { .ax_mmss = imu.ax_mmss, .ay_mmss = imu.ay_mmss, .az_mmss = imu.az_mmss };
                can_encode_accel(&f, &a);
                send(&f, "accel");
            }

            /* hal_read_ads1115() intentionally not sent as its own message
             * yet - the bring-up test reads it but nothing in the SED's
             * message dictionary covers what these 4 analog channels
             * physically represent on the Rev B board. Still read here so
             * its health bit is accurate in STATUS_DATA; add a proper data
             * message once the channel assignment is defined. */
            hal_ads1115_t ads = hal_read_ads1115();
            if (ads.ok) {
                last_ok_ads_ms = t;
                status |= CAN_INSTR_STATUS_ADS;
            }

            /* Sent every cycle regardless of which sensors are actually
             * populated on this board rev - this is what lets Master/ground
             * tell "not soldered down" and "soldered but failing" apart,
             * instead of inferring it from which data frames go missing. */
            can_instr_status_t st = { .sensor_status = status, .board_temp_c_x10 = board_temp_c_x10 };
            can_encode_status(&f, &st);
            send(&f, "status");

            /* Checked every 5s, but only ever logged if a sensor has
             * actually gone stale - dead quiet in steady state, one
             * consolidated error line the moment something's wrong. */
            if (util_every(t, &next_summary_ms, SUMMARY_PERIOD_MS)) {
                bool baro_ok = (t - last_ok_baro_ms) < SENSOR_STALE_MS;
                bool sht_ok  = (t - last_ok_sht_ms)  < SENSOR_STALE_MS;
                bool gas_ok  = (t - last_ok_gas_ms)  < SENSOR_STALE_MS;
                bool imu_ok  = (t - last_ok_imu_ms)  < SENSOR_STALE_MS;
                bool ads_ok  = (t - last_ok_ads_ms)  < SENSOR_STALE_MS;

                if (!(baro_ok && sht_ok && gas_ok && imu_ok && ads_ok)) {
                    ESP_LOGE(TAG, "sensor fault: baro=%s sht=%s gas=%s imu=%s ads=%s",
                             baro_ok ? "ok" : "FAIL", sht_ok ? "ok" : "FAIL",
                             gas_ok  ? "ok" : "FAIL", imu_ok ? "ok" : "FAIL",
                             ads_ok  ? "ok" : "FAIL");
                } else {
                    // All sensor data
                    ESP_LOGI(TAG, "Pressure (%dms) = %0.1f mbar, Altitude = %0.1f m, Temp = %0.1f C, Humidity = %0.1f%%, CO2 = %d ppm, IMU accel = (%d, %d, %d) mm/s^2",
                             (int)(t - last_ok_baro_ms), baro.pressure_mbar_x10 / 10.0f,
                             (float)lroundf(44330.0f * (1.0f - powf(baro.pressure_mbar_x10 / 10.0f / SEA_LEVEL_MBAR, 1.0f / 5.255f)) * 10.0f) / 10.0f,
                             baro.temp_c_x10 / 10.0f, sht.rh_pct_x10 / 10.0f, gas.co2_ppm,
                             imu.ax_mmss, imu.ay_mmss, imu.az_mmss);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif /* ESP_PLATFORM */