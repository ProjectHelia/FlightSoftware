#include "util_filter.h"

void util_ema_init(util_ema_t *f, float alpha) {
    f->alpha = (alpha > 0.0f && alpha <= 1.0f) ? alpha : 1.0f; /* NaN fails both */
    f->value = 0.0f;
    f->seeded = false;
}

float util_ema_update(util_ema_t *f, float sample) {
    if (!f->seeded) {
        f->value = sample;
        f->seeded = true;
    } else {
        f->value += f->alpha * (sample - f->value);
    }
    return f->value;
}