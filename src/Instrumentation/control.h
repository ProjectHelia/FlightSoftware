/**
 * @file control.h
 * @brief Polls the five I2C sensors and sends their readings onto CAN
 */
#ifndef HELIA_INSTRUMENTATION_CONTROL_H
#define HELIA_INSTRUMENTATION_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Pinned to core 1 by setup.c. ESP32-only (see control.c) -
 *         compiles to nothing on the host, same as every other node's
 *         control.c. */
void instr_control_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_INSTRUMENTATION_CONTROL_H */