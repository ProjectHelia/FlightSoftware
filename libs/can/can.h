/**
 * @file can.h
 * @brief Driver-independent CAN helpers: frame type, byte packing, and
 *        encode/decode for messages every node shares.
 *
 * Pure functions only. Sending and receiving frames is done by each node's
 * comms code through its HAL; this library never touches the bus.
 */
#ifndef HELIA_CAN_H
#define HELIA_CAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief One classic CAN frame, independent of ESP-IDF's twai_message_t. */
typedef struct {
    uint32_t id;               /**< 11-bit ID, see can_make_id(). */
    uint8_t  dlc;              /**< Payload length, 0-8. */
    uint8_t  data[CAN_MAX_DLC];
} can_frame_t;

/**
 * @brief Start a frame: sets the ID and length, zeroes the payload.
 * @param dlc Payload length; values above 8 are clamped to 8.
 */
void can_frame_init(can_frame_t *f, can_priority_t prio, can_source_t src,
                    uint8_t type, uint8_t dlc);

/** @name Little-endian byte packing */
/**@{*/
void     can_put_u16(uint8_t *buf, uint16_t v);
void     can_put_i16(uint8_t *buf, int16_t v);
void     can_put_u32(uint8_t *buf, uint32_t v);
uint16_t can_get_u16(const uint8_t *buf);
int16_t  can_get_i16(const uint8_t *buf);
uint32_t can_get_u32(const uint8_t *buf);
/**@}*/

/* ---- Heartbeat --------------------------------------------------------- */

/** @brief Heartbeat contents. Payload: [state u8][uptime_s u32] = 5 bytes. */
typedef struct {
    can_source_t source;   /**< Which node sent it (from the ID). */
    uint8_t      state;    /**< Sender's fsm_state_t, as a byte. */
    uint32_t     uptime_s; /**< Seconds since boot. A drop means the node rebooted. */
} can_heartbeat_t;

/** @brief Build a heartbeat frame (INFO priority, type 0x00). */
void can_encode_heartbeat(can_frame_t *f, const can_heartbeat_t *hb);

/**
 * @brief Decode a heartbeat.
 * @return false if the frame is not a well-formed heartbeat (wrong priority,
 *         wrong type, command-only source, or too short). @p out is untouched.
 */
bool can_decode_heartbeat(const can_frame_t *f, can_heartbeat_t *out);

/* ---- System commands (unlock / sleep / reset) -------------------------- */

/** @brief Commands every node must handle, whether broadcast or targeted. */
typedef enum {
    CAN_SYS_NONE = 0, /**< Frame is not a system command for this node. */
    CAN_SYS_UNLOCK,   /**< SAFE -> ACTIVE. */
    CAN_SYS_SLEEP,    /**< Enter SLEEP. */
    CAN_SYS_RESET,    /**< Reboot the MCU. */
} can_sys_cmd_t;

/**
 * @brief Build a system command frame (Master uses this).
 * @param target A node's source ID, or CAN_SRC_BROADCAST for the *_ALL version.
 * @return false if @p cmd is CAN_SYS_NONE or out of range.
 */
bool can_encode_sys_cmd(can_frame_t *f, can_sys_cmd_t cmd, can_source_t target);

/**
 * @brief Work out whether a frame is a system command meant for this node.
 *
 * Matches the full ID (priority included), so Master's HEARTBEAT (INFO, type
 * 0x00) is never mistaken for CMD_UNLOCK_NODE (IMPORTANT, type 0x00).
 *
 * @param me This node's source ID.
 * @return The command, or CAN_SYS_NONE if the frame is anything else.
 */
can_sys_cmd_t can_decode_sys_cmd(const can_frame_t *f, can_source_t me);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_H */