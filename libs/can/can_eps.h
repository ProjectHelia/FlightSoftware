/**
 * @file can_eps.h
 * @brief EPS message payloads. Shared so EPS encodes and Master decodes
 *        with the same code.
 *
 * EPS_STATUS payload (6 bytes, little-endian):
 *   [current_ma i16][shunt_mv_x100 i16][temp_c_x10 i16]
 * A value with no valid reading is sent as UTIL_I16_INVALID (INT16_MIN).
 */
#ifndef HELIA_CAN_EPS_H
#define HELIA_CAN_EPS_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief EPS_STATUS contents. */
typedef struct {
    int16_t current_ma;    /**< Input bus current, mA. */
    int16_t shunt_mv_x100; /**< Voltage across the shunt, units of 0.01 mV. */
    int16_t temp_c_x10;    /**< EPS PCB temperature, units of 0.1 degC. */
} can_eps_status_t;

/** @brief Encode EPS_STATUS. */
void can_encode_eps_status(can_frame_t *f, const can_eps_status_t *s);

/** @brief Decode EPS_STATUS. False if the frame isn't one. */
bool can_decode_eps_status(const can_frame_t *f, can_eps_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_EPS_H */