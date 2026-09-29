/**
 * @file util_timer.h
 * @brief Periodic timing helper for task loops that are given the current
 *        millisecond count.
 */
#ifndef HELIA_UTIL_TIMER_H
#define HELIA_UTIL_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief True once per @p period_ms, advancing @p next_ms to the next deadline.
 *
 * Correct across the millisecond counter wrapping (~49 days). Initialise
 * *next_ms to the current time to make the first call fire immediately.
 */
bool util_every(uint32_t now_ms, uint32_t *next_ms, uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_UTIL_TIMER_H */