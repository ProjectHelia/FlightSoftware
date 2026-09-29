/**
 * @file can_master.h
 * @brief Master status message payload. Shared so Master encodes and ground
 *        (or any node) decodes with the same layout :)
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
    uint32_t uptime_s;     /**< Seconds since boot */
    uint16_t uplink_cmds;  /**< TT&C uplink commands processed since boot */
    int8_t   chip_temp_c;  /**< Whole degrees C not floating */
    uint8_t  dl_drops;     /**< Downlink queue-overflow + no-link drops... Note this can saturate if errors enough */
} can_master_status_t;

/** @brief Encode MASTER_STATUS (INFO priority, source Master) */
void can_encode_master_status(can_frame_t *f, const can_master_status_t *s);

/** @brief Decode MASTER_STATUS */
bool can_decode_master_status(const can_frame_t *f, can_master_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_MASTER_H */