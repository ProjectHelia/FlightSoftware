#include "comms.h"
#include "util_timer.h"

#define ME CAN_SRC_INSTRUMENTATION

void instr_comms_init(instr_comms_t *c, uint32_t now_ms) {
    c->state = FSM_INIT;
    c->next_heartbeat_ms = now_ms;
    c->reset_requested = false;
}

void instr_comms_boot_done(instr_comms_t *c) {
    c->state = fsm_next(c->state, FSM_EVT_INIT_OK);
}

void instr_comms_on_frame(instr_comms_t *c, const can_frame_t *rx) {
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

        // SKIP SKIP When does this game get good
        case CAN_SYS_NONE:
            break;
    }
}

size_t instr_comms_on_tick(instr_comms_t *c, uint32_t now_ms, can_frame_t tx[INSTR_COMMS_MAX_TX]) {
    if (!util_every(now_ms, &c->next_heartbeat_ms, CAN_HEARTBEAT_PERIOD_MS))
        return 0;

    can_heartbeat_t hb = { ME, (uint8_t)c->state, now_ms / 1000u };
    can_encode_heartbeat(&tx[0], &hb);
    return 1;
}