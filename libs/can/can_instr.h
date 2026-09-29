/**
 * @file can_instr.h
 * @brief Instrumentation telemetry (source 0x5) encode/decode!
 * This is pure with no HAL or RTOS dependencies unlike the last codebase I had which got really messy
 */
#ifndef HELIA_CAN_INSTR_H
#define HELIA_CAN_INSTR_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_MSG_PRESSURE 0x01u
#define CAN_MSG_ALTITUDE 0x02u
#define CAN_MSG_HOUSEKEEPING 0x03u

// New (not in SED) messages for sensing
#define CAN_MSG_HUMIDITY 0x04u
#define CAN_MSG_GAS 0x05u
#define CAN_MSG_ACCEL 0x06u
#define CAN_MSG_STATUS 0x07u

#define CAN_INSTR_STATUS_BARO (1u << 0) /**< MS5611 */
#define CAN_INSTR_STATUS_SHT (1u << 1)  /**< SHT45 */
#define CAN_INSTR_STATUS_GAS (1u << 2)  /**< SCD41 */
#define CAN_INSTR_STATUS_IMU (1u << 3)  /**< BNO08x */
#define CAN_INSTR_STATUS_ADS (1u << 4)  /**< ADS1115 */

typedef struct {
    int32_t pressure_mbar_x10;
} can_instr_pressure_t;

typedef struct {
    int32_t altitude_m_x10;
} can_instr_altitude_t;

typedef struct {
    int32_t pressure_mbar_x10;
    int32_t altitude_m_x10;
} can_instr_housekeeping_t;

typedef struct {
    int16_t temp_c_x10;
    int16_t rh_pct_x10;
} can_instr_humidity_t;

typedef struct {
    uint16_t co2_ppm;
    int16_t temp_c_x10;
    int16_t rh_pct_x10;
} can_instr_gas_t;

typedef struct {
    int16_t ax_mmss;
    int16_t ay_mmss;
    int16_t az_mmss;
} can_instr_accel_t;

typedef struct {
    uint8_t sensor_status; /**< bitmask, CAN_INSTR_STATUS_* */
    int16_t board_temp_c_x10;
} can_instr_status_t;

void can_encode_pressure(can_frame_t *f, const can_instr_pressure_t *s);
bool can_decode_pressure(const can_frame_t *f, can_instr_pressure_t *out);

void can_encode_altitude(can_frame_t *f, const can_instr_altitude_t *s);
bool can_decode_altitude(const can_frame_t *f, can_instr_altitude_t *out);

void can_encode_housekeeping(can_frame_t *f, const can_instr_housekeeping_t *s);
bool can_decode_housekeeping(const can_frame_t *f, can_instr_housekeeping_t *out);

void can_encode_humidity(can_frame_t *f, const can_instr_humidity_t *s);
bool can_decode_humidity(const can_frame_t *f, can_instr_humidity_t *out);

void can_encode_gas(can_frame_t *f, const can_instr_gas_t *s);
bool can_decode_gas(const can_frame_t *f, can_instr_gas_t *out);

void can_encode_accel(can_frame_t *f, const can_instr_accel_t *s);
bool can_decode_accel(const can_frame_t *f, can_instr_accel_t *out);

void can_encode_status(can_frame_t *f, const can_instr_status_t *s);
bool can_decode_status(const can_frame_t *f, can_instr_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_INSTR_H */