/**
 * @file control.h
 * @brief turn raw EPS readings into scaled data for CAN
 */
#ifndef HELIA_EPS_CONTROL_H
#define HELIA_EPS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "util_filter.h"
#include "util_thermistor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EPS_SHUNT_OHMS 0.1f // Resistor value in ohms: shunt voltage / shunt resistance = current in amps (pookie recommended)
#define EPS_SHUNT_MV_MAX 81.9f

#define EPS_NTC_CFG                                                      \
    { .supply_mv = 5000.0f, .r_fixed_ohm = 10000.0f, .r0_ohm = 10000.0f, \
      .t0_c = 25.0f, .beta = 3950.0f, .ntc_on_top = true }

#define EPS_ADC_MAX_MV 3000.0f

#define EPS_TEMP_MIN_C -40.0f
#define EPS_TEMP_MAX_C 125.0f // double check

#define EPS_TEMP_ALPHA 0.2f

typedef struct {
    bool shunt_ok;
    float shunt_mv;
    bool therm_ok;
    float therm_mv;
} eps_raw_t;

typedef struct {
    int16_t current_ma;
    int16_t shunt_mv_x100;
    int16_t temp_c_x10;
} eps_data_t;

typedef struct {
    util_ntc_cfg_t ntc;
    util_ema_t temp_filter;
} eps_control_t;

void eps_control_init(eps_control_t *c);
void eps_data_init(eps_data_t *d);

void eps_control_step(eps_control_t *c, const eps_raw_t *raw, eps_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_EPS_CONTROL_H */