/**
 * @file comms.h
 * @brief Photonics communication logic: this is jsut heartbeat and state transitions. Check out control.c for actual photonics data
 * 
 * TODO(remy): add more detailed description of the comms logic, including state machine and heartbeat
 */
#ifndef HELIA_PHOTONICS_COMMS_H
#define HELIA_PHOTONICS_COMMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "can.h"
#include "fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHOTONICS_COMMS_MAX_TX 1

typedef struct {
    fsm_state_t state;
    uint32_t    next_heartbeat_ms;
    bool        reset_requested;
} photonics_comms_t;

void photonics_comms_init(photonics_comms_t *c, uint32_t now_ms);

void photonics_comms_boot_done(photonics_comms_t *c);

void photonics_comms_on_frame(photonics_comms_t *c, const can_frame_t *rx);

size_t photonics_comms_on_tick(photonics_comms_t *c, uint32_t now_ms, can_frame_t tx[PHOTONICS_COMMS_MAX_TX]);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_PHOTONICS_COMMS_H */