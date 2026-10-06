/**
 * @file hal.h
 * @brief Photonics HAL: CAN bus + a TCA9548A I2C mux carrying an unknown-
 *        ahead-of-time mix of LTR390 UV/ambient-light sensors and ADS1115
 *        ADCs (one per flask PCB, 4 photodiodes each).
 *
 * The board has grown since the first bring-up: it's no longer "2 fixed
 * LTR390s + 1 onboard ADC" - it's now N flask PCBs (each with its own
 * ADS1115 reading 4 photodiodes) plus the UV sensors, all multiplexed onto
 * the same two I2C addresses (0x53 LTR390, 0x48 ADS1115) via the mux, with
 * electrical still actively wiring more flasks up (3 of 4 as of writing).
 * Hardcoding "UV is on channel 0/1, ADC is onboard" would go stale the next
 * time a flask gets added or moved, so this HAL follows the same approach
 * as the Arduino prototype: scan every mux channel at boot, probe each one
 * to see what's actually sitting behind it (LTR390 vs ADS1115 vs nothing),
 * and build the channel map from that instead of assuming it.
 */
#ifndef HELIA_PHOTONICS_HAL_H
#define HELIA_PHOTONICS_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHOTONICS_MUX_CHANNELS 8 /**< TCA9548A channel count - every channel
                                       gets probed at boot, used or not. */
#define PHOTONICS_MAX_UV     2   /**< Ceiling on LTR390s the boot scan will
                                       assign a slot to. */
#define PHOTONICS_MAX_FLASKS 4   /**< Ceiling on flask ADS1115s the boot scan
                                       will assign a slot to - bump this (and
                                       add a matching CAN_MSG_FLASKx in
                                       can_photonics.h) as electrical wires up
                                       more; PHOTONICS_MUX_CHANNELS - MAX_UV
                                       is the hard limit the mux allows. */

/** @brief One LTR390's latest state. */
typedef struct {
    bool    ok;         /**< Sensor was found and initialised at boot. */
    bool    fresh;       /**< True only on a call that actually pulled new
                               data off the sensor (lux OR uvi updated this
                               call) - false ("nothing new this tick") is
                               normal and expected most calls, same as the
                               original bring-up test's newDataAvailable()
                               gate. */
    int32_t lux_x10;     /**< Ambient light, 0.1 lux units. Last known
                               value - only updated when the sensor was in
                               ALS mode on a fresh call. */
    int32_t uvi_x1000;   /**< UV index, 0.001 units. Last known value -
                               only updated when the sensor was in UVS mode
                               on a fresh call. */
} hal_uv_t;

/** @brief One flask's ADS1115: 4 single-ended channels, one per
 *         photodiode on that flask's PCB. */
typedef struct {
    bool    ok;     /**< This flask's ADS1115 was found at boot. */
    int16_t mv[4];
} hal_flask_t;

bool hal_init(void);
void hal_restart(void);

/** @brief How many LTR390s the boot scan actually found (<= PHOTONICS_MAX_UV).
 *         Use this instead of assuming PHOTONICS_MAX_UV are present. */
uint8_t hal_num_uv(void);

/** @brief How many flask ADS1115s the boot scan actually found (<=
 *         PHOTONICS_MAX_FLASKS) - e.g. 3 right now while electrical wires
 *         up the 4th. Use this instead of assuming PHOTONICS_MAX_FLASKS
 *         are present, so control.c doesn't have to know or care how many
 *         flasks are physically wired up today. */
uint8_t hal_num_flasks(void);

/** @brief Services UV sensor `idx` (0..hal_num_uv()-1): selects its mux
 *         channel, checks for new data, and if there is any, updates and
 *         returns its latest reading. Ping-pongs the sensor between ALS
 *         and UVS mode exactly like the original bring-up test - each
 *         fresh call flips it to the other mode for next time. Returns
 *         the latest known values either way; .ok is false only if the
 *         sensor was never found at boot - call it anyway on an
 *         out-of-range idx and you'll just get a zeroed, not-ok result. */
hal_uv_t hal_read_uv(uint8_t idx);

/** @brief Reads flask `idx`'s ADS1115 (0..hal_num_flasks()-1): selects its
 *         mux channel and reads all 4 photodiode channels in millivolts.
 *         .ok is false if that flask's ADC wasn't found at boot, or on an
 *         out-of-range idx. */
hal_flask_t hal_read_flask(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_PHOTONICS_HAL_H */