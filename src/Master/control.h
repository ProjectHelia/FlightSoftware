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
 *        created (i.e. before ttc_uplink_task) other we will have a race condition
 */
void master_control_init(void);

/** @brief Control core task: periodically checks the DESCENDING -> LANDED
 *         no-motion timeout */
void master_control_task(void *arg);

/**
 * @brief Thread-safe: apply a ground-commanded phase chang
 */
master_flight_result_t master_control_set_phase(master_phase_t next);

/** @brief Thread-safe read of the current flight phase */
master_phase_t master_control_get_phase(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_CONTROL_H */