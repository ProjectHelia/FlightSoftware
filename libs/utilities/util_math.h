/**
 * @file util_math.h
 * @brief Range checks, clamping, and float <-> scaled-integer conversion.
 */
#ifndef HELIA_UTIL_MATH_H
#define HELIA_UTIL_MATH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Scaled value meaning "no valid reading"
 * I don't think this is ever goign to be a problem tbh, might remove later
 */
#define UTIL_I16_INVALID INT16_MIN

/**
 * @brief True if lo <= v <= hi.
 * False for NaN, so a broken sensor fails the check
 * I know what you're thinking, this is a pretty dumb function. The thought behind it is readability but also to avoid common mistakes with comparisons that could risk fucking over the project.
 */
bool util_in_range(float v, float lo, float hi);

/** @brief Limit v to [lo, hi]. NaN returns lo. */
float util_clampf(float v, float lo, float hi);

/**
 * @brief Convert a physical value to a scaled int16 for CAN (SED: 37.2 degC x10 -> 372)
 * Rounds to nearest and saturates to [-32767, 32767]
 * NaN returns UTIL_I16_INVALID
 */
int16_t util_to_i16(float value, float scale);

/** @brief Inverse of util_to_i16() */
float util_from_i16(int16_t raw, float scale);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_UTIL_MATH_H */