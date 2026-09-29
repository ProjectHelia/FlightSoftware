#include "comms.h"
#include "can_eps.h"
#include "util_timer.h"

#define ME CAN_SRC_EPS

void eps_comms_init(eps_comms_t *c, uint32_t now_ms) {
    c->state = FSM_INIT;
    c->next_heartbeat_ms = now_ms;
    c->next_status_ms = now_ms;
    c->reset_requested = false;
}

void eps_comms_boot_done(eps_comms_t *c) {
    c->state = fsm_next(c->state, FSM_EVT_INIT_OK);
}

void eps_comms_on_frame(eps_comms_t *c, const can_frame_t *rx) {
    switch (can_decode_sys_cmd(rx, ME)) {
        case CAN_SYS_UNLOCK:
            c->state = fsm_next(c->state, FSM_EVT_UNLOCK);
            break;
        case CAN_SYS_SLEEP:
            c->state = fsm_next(c->state, FSM_EVT_SLEEP);
            break;
        case CAN_SYS_RESET:
            c->reset_requested = true;
            break;

        // base case
        case CAN_SYS_NONE:
            break;
    }
}

size_t eps_comms_on_tick(eps_comms_t *c, uint32_t now_ms,
    const eps_data_t *latest, can_frame_t tx[EPS_COMMS_MAX_TX]) {
    size_t n = 0;

    if (util_every(now_ms, &c->next_heartbeat_ms, CAN_HEARTBEAT_PERIOD_MS)) {
        can_heartbeat_t hb = { ME, (uint8_t)c->state, now_ms / 1000u };
        can_encode_heartbeat(&tx[n++], &hb);
    }

    if (c->state == FSM_ACTIVE && util_every(now_ms, &c->next_status_ms, EPS_STATUS_PERIOD_MS)) {
        can_eps_status_t s = { latest->current_ma, latest->shunt_mv_x100, latest->temp_c_x10 };
        can_encode_eps_status(&tx[n++], &s);
    }
    return n;
}