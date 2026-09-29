/**
 * @file util_filter.h
 * @brief Exponential moving average (EMA) filter for noisy sensors
 */
#ifndef HELIA_UTIL_FILTER_H
#define HELIA_UTIL_FILTER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief EMA state. */
typedef struct {
    float alpha;  /**< Smoothing factor, (0, 1) */
    float value;  /**< Current filtered output */
    bool  seeded; /**< False until the first sample arrives */
} util_ema_t;

/**
 * @brief Set up a filter. 
 * An alpha outside (0, 1] (or NaN) becomes 1.0,
 *        i.e. no filtering, rather than a filter that never moves
 */
void util_ema_init(util_ema_t *f, float alpha);

/**
 * @brief Add a sample and return the new filtered value. 
 * The first sample is taken as-is, so the output doesn't ramp up from zero
 * Range-check samples with util_in_range() before calling this just ot be sure
 */
float util_ema_update(util_ema_t *f, float sample);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_UTIL_FILTER_H */