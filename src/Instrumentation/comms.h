/**
 * @file comms.h
 * @brief for now only the lifecycle (every node has (heartbeat, unlock/sleep/reset)
 */
#ifndef HELIA_INSTRUMENTATION_COMMS_H
#define HELIA_INSTRUMENTATION_COMMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "can.h"
#include "fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INSTR_COMMS_MAX_TX 1

typedef struct {
    fsm_state_t state;
    uint32_t next_heartbeat_ms;
    bool reset_requested;
} instr_comms_t;

void instr_comms_init(instr_comms_t *c, uint32_t now_ms);
void instr_comms_boot_done(instr_comms_t *c);
void instr_comms_on_frame(instr_comms_t *c, const can_frame_t *rx);
size_t instr_comms_on_tick(instr_comms_t *c, uint32_t now_ms, can_frame_t tx[INSTR_COMMS_MAX_TX]);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_INSTRUMENTATION_COMMS_H */