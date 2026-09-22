/**
 * @file can_master.h
 * @brief Master status message payload. Shared so Master encodes and ground
 *        (or any node) decodes with the same layout.
 *
 * MASTER_STATUS payload (8 bytes, little-endian):
 *   [uptime_s u32][uplink_cmds u16][chip_temp_c i8][dl_drops u8]
 *
 * This replaces the TT&C bring-up firmware's ad hoc 8-byte "heartbeat"
 * (which reused CAN_MSG_HEARTBEAT for uptime/cmd-count/temp/drops instead
 * of the real 5-byte state+uptime heartbeat every node sends). Master now
 * sends both: the standard heartbeat via can_encode_heartbeat(), and this
 * as a separate message for the extra bring-up telemetry.
 */
#ifndef HELIA_CAN_MASTER_H
#define HELIA_CAN_MASTER_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief MASTER_STATUS contents. */
typedef struct {
    uint32_t uptime_s;     /**< Seconds since boot. */
    uint16_t uplink_cmds;  /**< TT&C uplink commands processed since boot. */
    int8_t   chip_temp_c;  /**< Whole degrees C. INT8_MIN = unavailable. */
    uint8_t  dl_drops;     /**< Downlink queue-overflow + no-link drops. Saturates at 255. */
} can_master_status_t;

/** @brief Encode MASTER_STATUS (INFO priority, source Master). */
void can_encode_master_status(can_frame_t *f, const can_master_status_t *s);

/** @brief Decode MASTER_STATUS. False if the frame isn't one. */
bool can_decode_master_status(const can_frame_t *f, can_master_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_MASTER_H */