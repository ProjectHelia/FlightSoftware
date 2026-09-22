/**
 * @file can_photonics.h
 * @brief Photonics telemetry (source 0x4) encode/decode. Pure - no
 *        FreeRTOS, no I2C - same tier as can_instr.c/can_eps.c.
 *
 * The SED's Photonics CAN Message Dictionary (0x01-0x06: PD_DATA,
 * PD_AVG_DATA, UV_STATUS, EVENT_EXPOSURE_*) describes the full flight
 * photodiode array (8 photodiodes x 8 flasks via a mux, 64 channels) and
 * an active UV exposure sequence (elapsed time + dose). What's actually on
 * this board right now is different hardware: two LTR390 ambient-light/UV
 * sensors and an onboard ADS1115 - so none of those SED message shapes
 * fit. Picked fresh type IDs after the SED's reserved range rather than
 * force the data into a mismatched shape; the SED needs a table update
 * once the real photodiode array comes online.
 */
#ifndef HELIA_CAN_PHOTONICS_H
#define HELIA_CAN_PHOTONICS_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_MSG_UV0 0x07u /**< NOT YET IN SED - LTR390 #0 (mux ch 0): lux + UV index */
#define CAN_MSG_UV1 0x08u /**< NOT YET IN SED - LTR390 #1 (mux ch 1): lux + UV index */
#define CAN_MSG_ADC 0x09u /**< NOT YET IN SED - onboard ADS1115, 4 raw channels (mV) */

/** @brief One LTR390's latest reading. Sent whenever either field is
 *         freshly updated - the other field just carries its last known
 *         value (see hal_read_uv()'s ping-pong doc comment). */
typedef struct {
    int32_t lux_x10;   /**< Ambient light, units of 0.1 lux. */
    int32_t uvi_x1000; /**< UV index, units of 0.001. */
} can_photonics_uv_t;

/** @brief Raw ADS1115 4-channel read, millivolts. */
typedef struct {
    int16_t mv[4];
} can_photonics_adc_t;

void can_encode_uv0(can_frame_t *f, const can_photonics_uv_t *s);
bool can_decode_uv0(const can_frame_t *f, can_photonics_uv_t *out);

void can_encode_uv1(can_frame_t *f, const can_photonics_uv_t *s);
bool can_decode_uv1(const can_frame_t *f, can_photonics_uv_t *out);

void can_encode_adc(can_frame_t *f, const can_photonics_adc_t *s);
bool can_decode_adc(const can_frame_t *f, can_photonics_adc_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_PHOTONICS_H */