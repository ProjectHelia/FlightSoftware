/**
 * @file protocol.h
 * @brief HELIA CAN protocol constants: the SED's 11-bit ID scheme and message
 *        dictionary. Single source of truth - if the SED changes, change it here.
 *
 * ID layout (standard 11-bit CAN, MSB -> LSB):
 *   [priority: 2 bits][source: 3 bits][message type: 6 bits]
 * Lower numeric ID wins arbitration, so CRITICAL (0) beats INFO (3).
 *
 * Multi-byte payload fields are little-endian. Sensor values are scaled
 * integers, never floats (SED: e.g. 37.2 degC is sent as 372).
 */
#ifndef HELIA_CAN_PROTOCOL_H
#define HELIA_CAN_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bus bitrate. SED requirement: <= 500 kBd. */
#define CAN_BITRATE 500000u

/** @brief Every node sends a heartbeat at 2 Hz (SED). */
#define CAN_HEARTBEAT_PERIOD_MS 500u
/** @brief A node is considered lost after 3 missed heartbeats. */
#define CAN_HEARTBEAT_TIMEOUT_MS (3u * CAN_HEARTBEAT_PERIOD_MS)

/** @brief Maximum payload of a classic CAN frame. */
#define CAN_MAX_DLC 8u

/** @brief Message priority (2 bits). Lower value = higher priority. */
typedef enum {
    CAN_PRIO_CRITICAL  = 0x0,
    CAN_PRIO_IMPORTANT = 0x1,
    CAN_PRIO_WARNING   = 0x2,
    CAN_PRIO_INFO      = 0x3,
} can_priority_t;

/** @brief Message source (3 bits). Numbering per SED Tables 4.51-4.58. */
typedef enum {
    CAN_SRC_GROUND          = 0x0, /**< Manual command from ground support. */
    CAN_SRC_MASTER          = 0x1, /**< OBDH/TT&C. */
    CAN_SRC_BROADCAST       = 0x2, /**< Commands every node acts on. */
    CAN_SRC_MECHANISMS      = 0x3,
    CAN_SRC_PHOTONICS       = 0x4,
    CAN_SRC_INSTRUMENTATION = 0x5,
    CAN_SRC_EPS             = 0x6,
    CAN_SRC_THERMAL         = 0x7,
} can_source_t;

/* ---- Message types (6 bits). Type IDs are only unique per source. ------ */

/** @name Every node */
/**@{*/
#define CAN_MSG_HEARTBEAT 0x00u /**< INFO, 2 Hz. */
/**@}*/

/** @name Broadcast commands (source 0x2) */
/**@{*/
#define CAN_CMD_UNLOCK_ALL   0x00u /**< IMPORTANT */
#define CAN_CMD_SLEEP_ALL    0x01u /**< IMPORTANT */
#define CAN_CMD_RESET_ALL    0x02u /**< CRITICAL */
#define CAN_CMD_FLIGHT_PHASE 0x03u /**< IMPORTANT */
#define CAN_CMD_TIME_SYNC    0x04u /**< IMPORTANT */
/**@}*/

/** @name Master per-node commands (source 0x1); data[0] = target node */
/**@{*/
#define CAN_CMD_UNLOCK_NODE 0x00u /**< IMPORTANT. Same type as Master HEARTBEAT: priority tells them apart. */
#define CAN_CMD_SLEEP_NODE  0x01u /**< IMPORTANT */
#define CAN_CMD_RESET_NODE  0x02u /**< CRITICAL */
/**@}*/

/** @name Master telemetry (source 0x1), see can_master.h for payload */
/**@{*/
#define CAN_MSG_MASTER_STATUS 0x10u /**< INFO, 1 Hz. Uptime, uplink activity, chip temp, link drops. */
/**@}*/

/** @name EPS messages (source 0x6), see can_eps.h for payloads */
/**@{*/
#define CAN_EPS_STATUS 0x04u /**< INFO, 1 Hz while ACTIVE. */
/**@}*/

/* Other nodes' message types are added here as each node is implemented,
 * so this file always matches what the code actually uses. */

/**
 * @brief Pack an 11-bit CAN ID. Out-of-range inputs are masked, never spill
 *        into neighbouring fields.
 */
static inline uint32_t can_make_id(can_priority_t prio, can_source_t src, uint8_t type)
{
    return (((uint32_t)prio & 0x3u) << 9) | (((uint32_t)src & 0x7u) << 6) | ((uint32_t)type & 0x3Fu);
}

/** @brief Priority field of an ID. */
static inline can_priority_t can_id_priority(uint32_t id) { return (can_priority_t)((id >> 9) & 0x3u); }
/** @brief Source field of an ID. */
static inline can_source_t can_id_source(uint32_t id) { return (can_source_t)((id >> 6) & 0x7u); }
/** @brief Message type field of an ID. */
static inline uint8_t can_id_type(uint32_t id) { return (uint8_t)(id & 0x3Fu); }

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_PROTOCOL_H */