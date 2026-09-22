/**
 * @file can_instr.h
 * @brief Instrumentation telemetry (source 0x5) encode/decode. Pure - no
 *        FreeRTOS, no I2C - same tier as can_master.c/can_eps.c.
 *
 * PRESSURE_DATA/ALTITUDE_DATA/HOUSEKEEPING_DATA (0x01-0x03) are in the SED's
 * Instrumentation CAN Message Dictionary. HUMIDITY_DATA/GAS_DATA/ACCEL_DATA/
 * STATUS_DATA (0x04-0x07) are NOT in the SED yet - the board carries an
 * SHT45, SCD41 and BNO08x that the current SED table doesn't have messages
 * for, and STATUS_DATA (sensor health bitmap + MS5611 board temp) is new.
 * Picked the next free type IDs; the SED needs a table update to match
 * before this is final.
 */
#ifndef HELIA_CAN_INSTR_H
#define HELIA_CAN_INSTR_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_MSG_PRESSURE     0x01u /**< per SED */
#define CAN_MSG_ALTITUDE     0x02u /**< per SED */
#define CAN_MSG_HOUSEKEEPING 0x03u /**< per SED: pressure + altitude, 1 Hz */
#define CAN_MSG_HUMIDITY     0x04u /**< NOT YET IN SED - SHT45 */
#define CAN_MSG_GAS          0x05u /**< NOT YET IN SED - SCD41 */
#define CAN_MSG_ACCEL        0x06u /**< NOT YET IN SED - BNO08x */
#define CAN_MSG_STATUS       0x07u /**< NOT YET IN SED - sensor health bitmap + MS5611 board temp */

/** @brief Bits in can_instr_status_t.sensor_status - set when that sensor
 *         read OK this poll cycle. A sensor that's simply not soldered
 *         down yet (SCD41/MS5611 on the current board rev) will always
 *         read its bit as 0, same as a genuinely failed one - this only
 *         tells you "did the last read succeed", not "should this be
 *         populated". */
#define CAN_INSTR_STATUS_BARO (1u << 0) /**< MS5611 */
#define CAN_INSTR_STATUS_SHT  (1u << 1) /**< SHT45 */
#define CAN_INSTR_STATUS_GAS  (1u << 2) /**< SCD41 */
#define CAN_INSTR_STATUS_IMU  (1u << 3) /**< BNO08x */
#define CAN_INSTR_STATUS_ADS  (1u << 4) /**< ADS1115 */

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
    int16_t  temp_c_x10;
    int16_t  rh_pct_x10;
} can_instr_gas_t;

typedef struct {
    int16_t ax_mmss;
    int16_t ay_mmss;
    int16_t az_mmss;
} can_instr_accel_t;

typedef struct {
    uint8_t sensor_status;    /**< bitmask, CAN_INSTR_STATUS_* */
    int16_t board_temp_c_x10; /**< MS5611's own temperature reading - a
                                    free cross-check against the SHT45's
                                    ambient reading. 0 when baro isn't
                                    present/reading OK; check the BARO
                                    status bit before trusting it. */
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