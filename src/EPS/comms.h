/**
 * @file comms.h
 * @brief Set's up heartbeat and state transitions for the EPS node
 */
#ifndef HELIA_EPS_COMMS_H
#define HELIA_EPS_COMMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "can.h"
#include "fsm.h"
#include "control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EPS_COMMS_MAX_TX     2     
#define EPS_STATUS_PERIOD_MS 1000u

typedef struct {
    fsm_state_t state;
    uint32_t    next_heartbeat_ms;
    uint32_t    next_status_ms;
    bool        reset_requested; // shit 
} eps_comms_t;


void eps_comms_init(eps_comms_t *c, uint32_t now_ms);

// Double checks we're in SAFE after boot
void eps_comms_boot_done(eps_comms_t *c);
void eps_comms_on_frame(eps_comms_t *c, const can_frame_t *rx);
size_t eps_comms_on_tick(eps_comms_t *c, uint32_t now_ms,
                         const eps_data_t *latest, can_frame_t tx[EPS_COMMS_MAX_TX]);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_EPS_COMMS_H */