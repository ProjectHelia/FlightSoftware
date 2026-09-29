#include "control.h"
#include "util_math.h"

void eps_control_init(eps_control_t *c) {
    c->ntc = (util_ntc_cfg_t)EPS_NTC_CFG;
    util_ema_init(&c->temp_filter, EPS_TEMP_ALPHA);
}

void eps_data_init(eps_data_t *d) {
    d->current_ma = UTIL_I16_INVALID;
    d->shunt_mv_x100 = UTIL_I16_INVALID;
    d->temp_c_x10 = UTIL_I16_INVALID;
}

void eps_control_step(eps_control_t *c, const eps_raw_t *raw, eps_data_t *out) {
    eps_data_init(out);

    // Filter isn't needed
    if (raw->shunt_ok && util_in_range(raw->shunt_mv, -EPS_SHUNT_MV_MAX, EPS_SHUNT_MV_MAX)) {
        out->shunt_mv_x100 = util_to_i16(raw->shunt_mv, 100.0f);
        out->current_ma = util_to_i16(raw->shunt_mv / EPS_SHUNT_OHMS, 1.0f); /* mV / ohm = mA */
    }

    if (raw->therm_ok && raw->therm_mv < EPS_ADC_MAX_MV) {
        float t = util_ntc_temp_c(&c->ntc, raw->therm_mv);
        if (util_in_range(t, EPS_TEMP_MIN_C, EPS_TEMP_MAX_C)) {
            out->temp_c_x10 = util_to_i16(util_ema_update(&c->temp_filter, t), 10.0f);
        }
    }
}