/**
 * @file hal.h
 * @brief Instrumentation hardware interface: CAN bus plus the five I2C
 *        sensors on the board:
 *       - SHT45 (temp/humidity)
 *       - ADS1115 (4-channel ADC)
 *       - SCD41 (CO2)
 *       - MS5611 (pressure)
 *       - FSP201 (accelerometer)
 *
 *
 */
#ifndef HELIA_INSTRUMENTATION_HAL_H
#define HELIA_INSTRUMENTATION_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool hal_init(void);
void hal_restart(void);

typedef struct {
    bool ok;
    int16_t temp_c_x10;
    int16_t rh_pct_x10;
} hal_sht45_t;

typedef struct {
    bool ok;
    int16_t mv[4];
} hal_ads1115_t;

typedef struct {
    bool ok;
    uint16_t co2_ppm;
    int16_t temp_c_x10;
    int16_t rh_pct_x10;
} hal_scd41_t;

typedef struct {
    bool ok;
    int32_t pressure_mbar_x10;
    int16_t temp_c_x10;
} hal_ms5611_t;

typedef struct {
    bool ok;
    int16_t ax_mmss;
    int16_t ay_mmss;
    int16_t az_mmss;
} hal_imu_t;

// Multiple of these don't work, needs debugging, but the code works for now
hal_sht45_t hal_read_sht45(void);
hal_ads1115_t hal_read_ads1115(void);
hal_scd41_t hal_read_scd41(void);
hal_ms5611_t hal_read_ms5611(void); // Not working

hal_imu_t hal_read_imu(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_INSTRUMENTATION_HAL_H */