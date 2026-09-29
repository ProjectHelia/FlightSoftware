#include "comms.h"
#include <stdio.h>
#include "can_eps.h"
#include "util_math.h"
#include "util_timer.h"

const char *master_node_name(can_source_t s) {
    switch (s) {
        case CAN_SRC_GROUND:
            return "GROUND";
        case CAN_SRC_MASTER:
            return "MASTER";
        case CAN_SRC_BROADCAST:
            return "BROADCAST";
        case CAN_SRC_MECHANISMS:
            return "MECHANISMS";
        case CAN_SRC_PHOTONICS:
            return "PHOTONICS";
        case CAN_SRC_INSTRUMENTATION:
            return "INSTRUMENTATION";
        case CAN_SRC_EPS:
            return "EPS";
        case CAN_SRC_THERMAL:
            return "THERMAL";
        default:
            return "?";
    }
}

void master_comms_init(master_comms_t *c, uint32_t now_ms) {
    /* Master has no one to unlock it, so it takes itself INIT -> SAFE -> ACTIVE.
     * TODO: gate the unlock on a ground command once TT&C exists. */
    c->state = fsm_next(FSM_INIT, FSM_EVT_INIT_OK);
    c->state = fsm_next(c->state, FSM_EVT_UNLOCK);
    c->next_heartbeat_ms = now_ms;
    for (int i = 0; i < MASTER_NODE_SLOTS; i++)
        c->last_seen_ms[i] = 0;
    c->alive_mask = 0;
}

size_t master_comms_on_frame(master_comms_t *c, uint32_t now_ms, const can_frame_t *rx,
    can_frame_t tx[MASTER_COMMS_MAX_TX]) {
    can_heartbeat_t hb;
    if (!can_decode_heartbeat(rx, &hb) || hb.source == CAN_SRC_MASTER)
        return 0;

    c->last_seen_ms[hb.source] = now_ms;
    c->alive_mask |= (uint8_t)(1u << hb.source);

    if (MASTER_AUTO_UNLOCK && hb.state == FSM_SAFE) {
        can_encode_sys_cmd(&tx[0], CAN_SYS_UNLOCK, hb.source);
        return 1;
    }
    return 0;
}

size_t master_comms_on_tick(master_comms_t *c, uint32_t now_ms,
    can_frame_t tx[MASTER_COMMS_MAX_TX]) {
    for (unsigned i = 0; i < MASTER_NODE_SLOTS; i++) {
        if ((c->alive_mask & (1u << i)) && now_ms - c->last_seen_ms[i] > CAN_HEARTBEAT_TIMEOUT_MS) {
            c->alive_mask &= (uint8_t) ~(1u << i);
        }
    }
    if (!util_every(now_ms, &c->next_heartbeat_ms, CAN_HEARTBEAT_PERIOD_MS))
        return 0;

    can_heartbeat_t hb = { CAN_SRC_MASTER, (uint8_t)c->state, now_ms / 1000u };
    can_encode_heartbeat(&tx[0], &hb);
    return 1;
}

/* Scaled value as text, or "invalid" for UTIL_I16_INVALID */
static const char *fmt_scaled(char *buf, size_t len, int16_t raw, float scale, const char *unit) {
    if (raw == UTIL_I16_INVALID)
        snprintf(buf, len, "invalid");
    else
        snprintf(buf, len, "%.2f %s", (double)util_from_i16(raw, scale), unit);
    return buf;
}

const char *master_describe_frame(const can_frame_t *f, char *buf, size_t len) {
    can_heartbeat_t hb;
    can_eps_status_t eps;
    char a[24], b[24], t[24];

    if (can_decode_heartbeat(f, &hb)) {
        snprintf(buf, len, "%s heartbeat: %s, up %lu s", master_node_name(hb.source),
            fsm_state_name((fsm_state_t)hb.state), (unsigned long)hb.uptime_s);
    } else if (can_decode_eps_status(f, &eps)) {
        snprintf(buf, len, "EPS status: current %s, shunt %s, PCB %s",
            fmt_scaled(a, sizeof a, eps.current_ma, 1.0f, "mA"),
            fmt_scaled(b, sizeof b, eps.shunt_mv_x100, 100.0f, "mV"),
            fmt_scaled(t, sizeof t, eps.temp_c_x10, 10.0f, "C"));
    } else {
        snprintf(buf, len, "%s frame: prio %u, type 0x%02X, %u bytes (id 0x%03lX)",
            master_node_name(can_id_source(f->id)), (unsigned)can_id_priority(f->id),
            can_id_type(f->id), f->dlc, (unsigned long)f->id);
    }
    return buf;
}