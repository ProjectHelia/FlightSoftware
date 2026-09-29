#include "util_math.h"
#include <math.h>

bool util_in_range(float v, float lo, float hi) {
    return v >= lo && v <= hi; /* every comparison with NaN is false */
}

float util_clampf(float v, float lo, float hi) {
    if (!(v >= lo))
        return lo; /* also catches NaN */
    if (v > hi)
        return hi;
    return v;
}

int16_t util_to_i16(float value, float scale) {
    float scaled = value * scale;
    if (isnan(scaled))
        return UTIL_I16_INVALID;

    /* Clamp BEFORE casting: casting an out-of-range float to int is undefined */
    scaled = util_clampf(roundf(scaled), (float)(INT16_MIN + 1), (float)INT16_MAX);
    return (int16_t)scaled;
}

float util_from_i16(int16_t raw, float scale) {
    if (raw == UTIL_I16_INVALID || scale == 0.0f)
        return NAN;
    return (float)raw / scale;
}