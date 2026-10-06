/**
 * @file can_photonics.h
 * @brief Photonics telemetry (source 0x4) encode/decode. Pure - no
 *        FreeRTOS, no I2C - same tier as can_instr.c/can_eps.c.
 *
 * The SED's Photonics CAN Message Dictionary (0x01-0x06: PD_DATA,
 * PD_AVG_DATA, UV_STATUS, EVENT_EXPOSURE_*) describes the full flight
 * photodiode array (8 photodiodes x 8 flasks via a mux, 64 channels) and
 * an active UV exposure sequence (elapsed time + dose). What's actually on
 * the bench right now is a step toward that: up to PHOTONICS_MAX_FLASKS
 * flask PCBs (4 photodiodes each, read via that flask's own ADS1115) plus
 * two LTR390 ambient-light/UV sensors, all auto-detected behind the same
 * TCA9548A mux (see Photonics/HAL/hal.h) - still a different shape to the
 * SED's 8x8 array, so these keep their own fresh type IDs after the SED's
 * reserved range. The SED needs a table update once the real 8-flask,
 * 8-photodiode-each array comes online.
 */
#ifndef HELIA_CAN_PHOTONICS_H
#define HELIA_CAN_PHOTONICS_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_MSG_UV0    0x07u /**< NOT YET IN SED - LTR390 UV slot 0: lux + UV index */
#define CAN_MSG_UV1    0x08u /**< NOT YET IN SED - LTR390 UV slot 1: lux + UV index */
#define CAN_MSG_FLASK0 0x09u /**< NOT YET IN SED - flask slot 0's ADS1115, 4 photodiode channels (mV) */
#define CAN_MSG_FLASK1 0x0Au /**< NOT YET IN SED - flask slot 1 */
#define CAN_MSG_FLASK2 0x0Bu /**< NOT YET IN SED - flask slot 2 */
#define CAN_MSG_FLASK3 0x0Cu /**< NOT YET IN SED - flask slot 3 */
/* Adding a 5th+ flask slot (Photonics/HAL/hal.h's PHOTONICS_MAX_FLASKS)
 * needs a CAN_MSG_FLASK4 here too, plus a matching encode/decode pair and
 * switch case in can_encode_flask()/can_decode_flask() below. */

/** @brief One LTR390's latest reading. Sent whenever either field is
 *         freshly updated - the other field just carries its last known
 *         value (see hal_read_uv()'s ping-pong doc comment). */
typedef struct {
    int32_t lux_x10;   /**< Ambient light, units of 0.1 lux. */
    int32_t uvi_x1000; /**< UV index, units of 0.001. */
} can_photonics_uv_t;

/** @brief One flask's ADS1115, 4-channel read, millivolts - one value per
 *         photodiode on that flask's PCB. */
typedef struct {
    int16_t mv[4];
} can_photonics_flask_t;

void can_encode_uv0(can_frame_t *f, const can_photonics_uv_t *s);
bool can_decode_uv0(const can_frame_t *f, can_photonics_uv_t *out);

void can_encode_uv1(can_frame_t *f, const can_photonics_uv_t *s);
bool can_decode_uv1(const can_frame_t *f, can_photonics_uv_t *out);

/** @brief Encodes flask `slot`'s reading (0..3 today - see
 *         PHOTONICS_MAX_FLASKS). Asserts/no-ops on an out-of-range slot
 *         rather than silently mislabeling a frame. */
void can_encode_flask(uint8_t slot, can_frame_t *f, const can_photonics_flask_t *s);

/** @brief Decodes any of the flask messages and reports which slot it
 *         came from via *slot_out. Returns false if `f` isn't one of the
 *         known CAN_MSG_FLASKn types. */
bool can_decode_flask(const can_frame_t *f, uint8_t *slot_out, can_photonics_flask_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_PHOTONICS_H */