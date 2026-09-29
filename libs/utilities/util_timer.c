#include "util_timer.h"

bool util_every(uint32_t now_ms, uint32_t *next_ms, uint32_t period_ms) {
    if ((int32_t)(now_ms - *next_ms) < 0)
        return false;
    *next_ms = now_ms + period_ms;
    return true;
}