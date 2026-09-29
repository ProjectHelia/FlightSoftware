/**
 * @file comms.h
 * @brief Master communication logic, currently a bench "bus tester":
 *   - sends Master's heartbeat every 500 ms
 *   - tracks which nodes are alive from their heartbeats
 *   - unlocks any node whose heartbeat says it is in SAFE
 *   - turns every received frame into a readable log line
 *
 */
#ifndef HELIA_MASTER_COMMS_H
#define HELIA_MASTER_COMMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "can.h"
#include "fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_COMMS_MAX_TX 1 /**< Most frames one call can ask to send. */
#define MASTER_NODE_SLOTS   8 /**< One per CAN source ID (3 bits). */

/**
 * @brief Unlock nodes automatically when they report SAFE.
 * Bench testing only: in flight, unlocking will follow ground commands and
 * flight phases.
 */
#define MASTER_AUTO_UNLOCK 1

/** @brief Comms state. Owned by the comms task. */
typedef struct {
    fsm_state_t state;
    uint32_t    next_heartbeat_ms;
    uint32_t    last_seen_ms[MASTER_NODE_SLOTS]; /**< Last heartbeat time per source. */
    uint8_t     alive_mask;                      /**< Bit n set = source n heard within the timeout. */
} master_comms_t;

/** @brief Start up. Master has no one to unlock it, so it goes straight to ACTIVE. */
void master_comms_init(master_comms_t *c, uint32_t now_ms);

/**
 * @brief Handle one received frame: note heartbeats, unlock nodes in SAFE.
 * @return Frames to send in reply.
 */
size_t master_comms_on_frame(master_comms_t *c, uint32_t now_ms, const can_frame_t *rx,
                             can_frame_t tx[MASTER_COMMS_MAX_TX]);

/**
 * @brief Periodic work: Master's heartbeat, and dropping nodes silent for
 *        longer than CAN_HEARTBEAT_TIMEOUT_MS from alive_mask.
 * @return Frames written to @p tx.
 */
size_t master_comms_on_tick(master_comms_t *c, uint32_t now_ms,
                            can_frame_t tx[MASTER_COMMS_MAX_TX]);

/** @brief Node name for logs, e.g. "EPS". */
const char *master_node_name(can_source_t s);

/**
 * @brief Describe a frame in words, e.g. "EPS heartbeat: SAFE, up 12 s".
 * @return @p buf, always NUL-terminated.
 */
const char *master_describe_frame(const can_frame_t *f, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_COMMS_H */