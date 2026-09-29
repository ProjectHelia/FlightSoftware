#include "util_control.h"
#include <math.h>

void util_bangbang_init(util_bangbang_t *c, float setpoint, float hysteresis) {
    c->setpoint = setpoint;
    c->hysteresis = (hysteresis > 0.0f) ? hysteresis : 0.0f;
    c->on = false;
}

bool util_bangbang_update(util_bangbang_t *c, float measured) {
    if (isnan(measured)) {
        c->on = false;
    } else if (measured <= c->setpoint - c->hysteresis) {
        c->on = true;
    } else if (measured >= c->setpoint + c->hysteresis) {
        c->on = false;
    }
    /* otherwise inside the dead band: keep previous state */
    return c->on;
}