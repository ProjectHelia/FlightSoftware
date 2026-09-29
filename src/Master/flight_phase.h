/**
 * @file flight_phase.h
 * @brief Master's flight-phase state machine. Pure logic - no FreeRTOS, no
 *        CAN, no sockets - so it's host-testable the same way master_comms
 *        is. Owning/synchronising an instance across cores is control.c's
 *        job, not this module's.
 *
 * Phases (per the SED's flight-phase table, names adapted): TESTING (SED's
 * "Pre-launch") -> STANDBY ("Launch") -> ASCENDING ("Ascent") -> FLOAT ->
 * DESCENDING ("Descent") -> LANDED ("Landing").
 *
 * NOTE ON A KNOWN SED DISCREPANCY: the SED's table lists Ascent (100m
 * altitude) and Float (apogee) as automatic transitions. Per team decision
 * this implementation makes every transition except the last
 * ground-commanded instead (simpler, no dependency on an altitude reading
 * Master doesn't itself have) - only DESCENDING -> LANDED is automatic
 * here, matching the SED's "automatic based on altitude rate of change"
 * intent via a no-motion timeout instead. The SED table should be updated
 * to match, or this should be revisited, before flight.
 *
 * All ground-commanded transitions go through master_flight_set_phase(),
 * which is deliberately permissive: the SED itself calls this command an
 * "override", so an out-of-sequence request (skipping a phase, or going
 * backward) is still applied, just reported back as
 * MASTER_FLIGHT_OVERRIDE instead of MASTER_FLIGHT_OK so the caller can log
 * it distinctly. Only a genuinely invalid phase value or a no-op
 * (requesting the phase we're already in) is rejected.
 */
#ifndef HELIA_MASTER_FLIGHT_PHASE_H
#define HELIA_MASTER_FLIGHT_PHASE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MASTER_PHASE_TESTING = 0,
    MASTER_PHASE_STANDBY,
    MASTER_PHASE_ASCENDING,
    MASTER_PHASE_FLOAT,
    MASTER_PHASE_DESCENDING,
    MASTER_PHASE_LANDED,
    MASTER_PHASE_COUNT,
} master_phase_t;

typedef enum {
    MASTER_FLIGHT_OK,       /**< applied, matched the expected next phase */
    MASTER_FLIGHT_OVERRIDE, /**< applied, but out of the normal sequence */
    MASTER_FLIGHT_REJECTED, /**< not applied: invalid phase, or already there */
} master_flight_result_t;

/** @brief Auto-transition DESCENDING -> LANDED after this long with no
 *         motion noted via master_flight_note_motion()
 * Currently this is set to 5 minutes... It might be a little bit longer lmao (5 mins for testing) 
 * */
#define MASTER_LANDING_NO_MOTION_MS (5u * 60u * 1000u)

typedef struct {
    master_phase_t phase;
    uint32_t phase_entered_ms; /**< now_ms() at last transition */
    uint32_t last_motion_ms;   /**< now_ms() at last noted motion (or phase entry) */
} master_flight_t;

/** @brief Starts in MASTER_PHASE_TESTING */
void master_flight_init(master_flight_t *f, uint32_t now_ms);

master_phase_t master_flight_phase(const master_flight_t *f);

/** @brief Human-readable phase name for logging */
const char *master_phase_name(master_phase_t p);


master_flight_result_t master_flight_set_phase(master_flight_t *f, master_phase_t next, uint32_t now_ms);
void master_flight_tick(master_flight_t *f, uint32_t now_ms);

/**
 * @brief Feeds the motion time out for landing detection.
 * TODO: Nothing calls this, need to use CAN frames from instrumentation to update this (blocked)
 */
void master_flight_note_motion(master_flight_t *f, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_FLIGHT_PHASE_H */