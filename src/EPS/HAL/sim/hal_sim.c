/* EPS HAL for running on a laptop: plausible readings with a little wobble,
 * so everything above the HAL behaves as it would on the board. */
#include "hal.h"
#include "control.h" /* EPS_NTC_CFG, so the sim always matches the real divider */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SIM_SHUNT_MV  35.0f /* ~350 mA through 0.1 ohm */
#define SIM_PCB_TEMP_C 30.0f

static unsigned s_tick;

static float wobble(void) /* repeating -0.5% .. +0.5% */
{
    return (float)(s_tick++ % 11u) * 0.001f - 0.005f;
}

/* The divider voltage the real circuit would produce at temp_c */
static float divider_mv(float temp_c)
{
    const util_ntc_cfg_t c = EPS_NTC_CFG;
    float r_ntc = c.r0_ohm * expf(c.beta * (1.0f / (temp_c + 273.15f) - 1.0f / (c.t0_c + 273.15f)));
    return c.ntc_on_top ? c.supply_mv * c.r_fixed_ohm / (r_ntc + c.r_fixed_ohm)
                        : c.supply_mv * r_ntc / (r_ntc + c.r_fixed_ohm);
}

bool hal_init(void)
{
    /* The simulated CAN bus is added with the simulator step. */
    return true;
}

bool hal_get_shunt_mv(float *out)
{
    *out = SIM_SHUNT_MV * (1.0f + wobble());
    return true;
}

bool hal_get_thermistor_mv(float *out)
{
    *out = divider_mv(SIM_PCB_TEMP_C) * (1.0f + wobble());
    return true;
}

void hal_restart(void)
{
    printf("[sim] restart requested\n");
    exit(0);
}