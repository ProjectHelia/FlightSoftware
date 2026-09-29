#include "fsm.h"

fsm_state_t fsm_next(fsm_state_t cur, fsm_event_t evt) {
    switch (cur) {
        case FSM_INIT:
            return (evt == FSM_EVT_INIT_OK) ? FSM_SAFE : FSM_INIT;

        case FSM_SAFE:
            if (evt == FSM_EVT_UNLOCK)
                return FSM_ACTIVE;
            if (evt == FSM_EVT_SLEEP)
                return FSM_SLEEP;
            return FSM_SAFE;

        case FSM_ACTIVE:
            return (evt == FSM_EVT_SLEEP) ? FSM_SLEEP : FSM_ACTIVE;

        case FSM_SLEEP:
            return FSM_SLEEP; /* only a reboot (GPIO wake) leaves SLEEP */

        default:
            return FSM_SAFE; /* corrupted state: fail safe */
    }
}

const char *fsm_state_name(fsm_state_t s) {
    switch (s) {
        case FSM_INIT:
            return "INIT";
        case FSM_SAFE:
            return "SAFE";
        case FSM_ACTIVE:
            return "ACTIVE";
        case FSM_SLEEP:
            return "SLEEP";
        default:
            return "UNKNOWN";
    }
}