#include "flight_phase.h"

static const char *PHASE_NAMES[MASTER_PHASE_COUNT] = {
    [MASTER_PHASE_TESTING] = "TESTING",
    [MASTER_PHASE_STANDBY] = "STANDBY",
    [MASTER_PHASE_ASCENDING] = "ASCENDING",
    [MASTER_PHASE_FLOAT] = "FLOAT",
    [MASTER_PHASE_DESCENDING] = "DESCENDING",
    [MASTER_PHASE_LANDED] = "LANDED", // This probably won't ever be sent over TT&C because radio connection will be lost
};

void master_flight_init(master_flight_t *f, uint32_t now_ms) {
    f->phase = MASTER_PHASE_TESTING;
    f->phase_entered_ms = now_ms;
    f->last_motion_ms = now_ms;
}

master_phase_t master_flight_phase(const master_flight_t *f) {
    return f->phase;
}

const char *master_phase_name(master_phase_t p) {
    if ((unsigned)p >= MASTER_PHASE_COUNT)
        return "UNKNOWN";
    return PHASE_NAMES[p];
}

master_flight_result_t master_flight_set_phase(master_flight_t *f, master_phase_t next, uint32_t now_ms) {
    if ((unsigned)next >= MASTER_PHASE_COUNT)
        return MASTER_FLIGHT_REJECTED;
    if (next == f->phase)
        return MASTER_FLIGHT_REJECTED; /* no-op */

    const bool sequential = ((unsigned)next == (unsigned)f->phase + 1u);

    f->phase = next;
    f->phase_entered_ms = now_ms;
    f->last_motion_ms = now_ms; /* fresh phase, fresh no-motion window */

    return sequential ? MASTER_FLIGHT_OK : MASTER_FLIGHT_OVERRIDE;
}

void master_flight_tick(master_flight_t *f, uint32_t now_ms) {
    if (f->phase != MASTER_PHASE_DESCENDING)
        return;
    if (now_ms - f->last_motion_ms < MASTER_LANDING_NO_MOTION_MS)
        return;

    f->phase = MASTER_PHASE_LANDED;
    f->phase_entered_ms = now_ms;
    f->last_motion_ms = now_ms;
}

void master_flight_note_motion(master_flight_t *f, uint32_t now_ms) {
    f->last_motion_ms = now_ms;
}