/**
 * @file hal.h
 * @brief EPS hardware interface. Implemented by HAL/esp32/hal_esp32.c on the
 *        board and HAL/sim/hal_sim.c on a laptop; the build links one of them.
 *
 * Functions return raw physical measurements. Turning those into current
 * and temperature is done in control.c, where it can be unit-tested.
 */
#ifndef HELIA_EPS_HAL_H
#define HELIA_EPS_HAL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bring up I2C, the INA226, the thermistor ADC and the CAN bus.
 *  @return false if any of them failed (the node keeps running regardless). */
bool hal_init(void);

/** @brief Voltage across the input-bus shunt resistor, in mV (signed). False if the INA226 didn't answer. */
bool hal_get_shunt_mv(float *out);

/** @brief Voltage at the PCB thermistor's ADC pin, in mV. False if the ADC read failed. */
bool hal_get_thermistor_mv(float *out);

/** @brief Reboot the node (CMD_RESET_*). Does not return. */
void hal_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_EPS_HAL_H */