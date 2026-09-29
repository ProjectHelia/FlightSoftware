/**
 * @file util_control.h
 * @brief Bang-bang (on/off) controller with hysteresis. PID goes here later.
 *
 * Output turns ON at or below (setpoint - hysteresis), OFF at or above
 * (setpoint + hysteresis), and holds its previous state in between, so a
 * heater doesn't chatter on and off around the setpoint.
 */
#ifndef HELIA_UTIL_CONTROL_H
#define HELIA_UTIL_CONTROL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Controller state. Change setpoint directly (e.g. on CMD_SET_TARGET) */
typedef struct {
    float setpoint;   /**< Target value, e.g. 35.0 degC */
    float hysteresis; /**< Half-width of the dead band, e.g. 0.5 degC */
    bool  on;         /**< Current output. */
} util_bangbang_t;

/** @brief Set up a controller
 * Output starts OFF
 * Negative hysteresis is treated as 0 */
void util_bangbang_init(util_bangbang_t *c, float setpoint, float hysteresis);

/**
 * @brief Feed a measurement, get the new output pretty much
 * A NaN measurement turns the output OFF
 */
bool util_bangbang_update(util_bangbang_t *c, float measured);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_UTIL_CONTROL_H */