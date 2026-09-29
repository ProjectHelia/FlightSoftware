#include "util_thermistor.h"
#include <math.h>
#include <stddef.h>

#define KELVIN 273.15f

float util_ntc_temp_c(const util_ntc_cfg_t *cfg, float adc_mv)
{
    if (cfg == NULL) return NAN;
    if (!(cfg->supply_mv > 0.0f && cfg->r_fixed_ohm > 0.0f &&
          cfg->r0_ohm > 0.0f && cfg->beta > 0.0f)) return NAN;

    /* At either rail the thermistor is open or shorted: no information */
    if (!(adc_mv > 0.0f && adc_mv < cfg->supply_mv)) return NAN;

    /* Solve the divider for the NTC's resistance */
    float r_ntc = cfg->ntc_on_top
        ? cfg->r_fixed_ohm * (cfg->supply_mv - adc_mv) / adc_mv
        : cfg->r_fixed_ohm * adc_mv / (cfg->supply_mv - adc_mv);

    float inv_t = 1.0f / (cfg->t0_c + KELVIN) + logf(r_ntc / cfg->r0_ohm) / cfg->beta;
    return 1.0f / inv_t - KELVIN;
}