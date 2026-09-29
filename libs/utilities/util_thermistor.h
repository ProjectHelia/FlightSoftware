/**
 * @file util_thermistor.h
 * @brief Convert an NTC thermistor voltage-divider reading to degC using the
 *        Beta equation. Shared: EPS, Master, Mechanisms and Thermals all
 *        have NTC thermistors.
 *
 * Beta equation: 1/T = 1/T0 + ln(R/R0)/B   (T in kelvin)
 */
#ifndef HELIA_UTIL_THERMISTOR_H
#define HELIA_UTIL_THERMISTOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Describes one thermistor circuit. 
 * Values are random bullshit AI made up because there's alike 5 different thermistors on the procurement sheet and none of them are right
 * */
typedef struct {
    float supply_mv;   
    float r_fixed_ohm; 
    float r0_ohm;     
    float t0_c;       
    float beta;        
    bool  ntc_on_top;  
} util_ntc_cfg_t;

/**
 * @brief Temperature in degC from the voltage at the ADC pin
 * @return NaN if the reading is at or beyond either rail (open or shorted
 *         thermistor) or the config is invalid. Please range check the output before using it! - Remy
 */
float util_ntc_temp_c(const util_ntc_cfg_t *cfg, float adc_mv);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_UTIL_THERMISTOR_H */