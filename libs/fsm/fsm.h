/**
 * @file fsm.h
 * @brief Node state machine shared by every node, per SED:
 *        INIT -> SAFE -> ACTIVE, and SAFE/ACTIVE <-> SLEEP.
 *
 */
#ifndef HELIA_FSM_H
#define HELIA_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Node states */
typedef enum {
    FSM_INIT = 0, /**< Booting, checking sensors and CAN */
    FSM_SAFE,     /**< Waiting for unlock; actuators must stay off to prevent shit going crazy */
    FSM_ACTIVE,   /**< Main control loop running */
    FSM_SLEEP,    /**< Entering deep sleep; wake is a reboot back into INIT */
    FSM_STATE_COUNT
} fsm_state_t;

/** @brief Events that can move the state machine. */
typedef enum {
    FSM_EVT_NONE = 0, /**< Nothing happened */
    FSM_EVT_INIT_OK,  /**< INIT checks passed */
    FSM_EVT_UNLOCK,   /**< CMD_UNLOCK_ALL, or CMD_UNLOCK_NODE for this node */
    FSM_EVT_SLEEP,    /**< CMD_SLEEP_ALL, or CMD_SLEEP_NODE for this node */
    FSM_EVT_COUNT
} fsm_event_t;

/**
 * @brief Compute the next state, invalid transitions leave the state unchanged
 * @param cur Current state: an out-of-range value (e.g. a bit flip) returns FSM_SAFE.
 * @param evt Event to apply
 * @return The next state
 */
fsm_state_t fsm_next(fsm_state_t cur, fsm_event_t evt);

/** @brief Short name for logging */
const char *fsm_state_name(fsm_state_t s);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_FSM_H */