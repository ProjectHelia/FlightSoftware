#ifndef HELIA_MASTER_CONTROL_H
#define HELIA_MASTER_CONTROL_H

#include "flight_phase.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the flight-phase state and its mutex. MUST be called once
 *        from app_main(), before any task that might call
 *        master_control_set_phase()/master_control_get_phase() is
 *        created (i.e. before ttc_uplink_task) - this avoids a boot-time
 *        race where a ground command could arrive before the mutex
 *        exists. master_control_task() itself only ticks; it doesn't
 *        create state, so its own creation order relative to this
 *        doesn't matter, only relative to ttc_uplink_task's.
 */
void master_control_init(void);

/** @brief Control core task: periodically checks the DESCENDING -> LANDED
 *         no-motion timeout. Pinned to core 1 by setup.c. ESP32-only (see
 *         control.c); declared unconditionally here since only ESP32-only
 *         callers reference it. Requires master_control_init() to have
 *         run first. */
void master_control_task(void *arg);

/**
 * @brief Thread-safe: apply a ground-commanded phase change. Called from
 *        ttc.c's uplink handler (core 0) when a CAN_CMD_FLIGHT_PHASE
 *        uplink command arrives - this is the flight manager's gate
 *        between ground commands and the rest of the system for that
 *        command. Broadcasts CAN_CMD_FLIGHT_PHASE to the bus and logs the
 *        result internally; the caller just needs the result to ACK
 *        ground correctly.
 *
 *        Safe to call even before master_control_task's first loop
 *        iteration, but NOT before it has been created (the mutex it uses
 *        is created at task start) - setup.c creates control before ttc's
 *        uplink task, so this is naturally satisfied.
 */
master_flight_result_t master_control_set_phase(master_phase_t next);

/** @brief Thread-safe read of the current flight phase. */
master_phase_t master_control_get_phase(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_CONTROL_H */