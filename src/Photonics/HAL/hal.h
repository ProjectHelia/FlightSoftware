/**
 * @file hal.h
 * @brief Photonics HAL:  two LTR390 UV/ambient-light sensors
 *       and one ADS1115 ADC, plus the mux that switches between the two! This is just for the EAR demo, full system should be written after pookie is done testing
 */
#ifndef HELIA_PHOTONICS_HAL_H
#define HELIA_PHOTONICS_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHOTONICS_NUM_UV 2

/** @brief LTR390 Sensor struct */
typedef struct {
    bool ok; // true if sensor was found at init
    bool fresh; // true if data actually changed, depends on how we want to measure photonics but added for now to avoid data duplication
    int32_t lux_x10;  // ambient light, scaled by 10 to avoid floating point
    int32_t uvi_x1000; // UV index, scaled by 1000 to avoid floating point
} hal_uv_t;

/** @brief Onboard ADS1115, 4 single-ended channels */
typedef struct {
    bool    ok; // true if ADC was found at init
    int16_t mv[4];
} hal_adc_t;

bool hal_init(void);
void hal_restart(void);

/** @brief add brief TODO */
hal_uv_t hal_read_uv(uint8_t idx);

/** @brief Reads the ADS1115 (check struct) */
hal_adc_t hal_read_adc(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_PHOTONICS_HAL_H */