#include "can.h"
#include <string.h>

void can_frame_init(can_frame_t *f, can_priority_t prio, can_source_t src,
                    uint8_t type, uint8_t dlc)
{
    memset(f, 0, sizeof(*f));
    f->id  = can_make_id(prio, src, type);
    f->dlc = (dlc > CAN_MAX_DLC) ? (uint8_t)CAN_MAX_DLC : dlc;
}

void can_put_u16(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)(v & 0xFFu);
    buf[1] = (uint8_t)(v >> 8);
}

void can_put_i16(uint8_t *buf, int16_t v) { can_put_u16(buf, (uint16_t)v); }

void can_put_u32(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v & 0xFFu);
    buf[1] = (uint8_t)((v >> 8) & 0xFFu);
    buf[2] = (uint8_t)((v >> 16) & 0xFFu);
    buf[3] = (uint8_t)(v >> 24);
}

uint16_t can_get_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | (uint16_t)((uint16_t)buf[1] << 8));
}

int16_t can_get_i16(const uint8_t *buf) { return (int16_t)can_get_u16(buf); }

uint32_t can_get_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

#define HEARTBEAT_DLC 5u

void can_encode_heartbeat(can_frame_t *f, const can_heartbeat_t *hb)
{
    can_frame_init(f, CAN_PRIO_INFO, hb->source, CAN_MSG_HEARTBEAT, HEARTBEAT_DLC);
    f->data[0] = hb->state;
    can_put_u32(&f->data[1], hb->uptime_s);
}

bool can_decode_heartbeat(const can_frame_t *f, can_heartbeat_t *out)
{
    if (f == NULL || out == NULL) return false;

    can_source_t src = can_id_source(f->id);
    if (can_id_priority(f->id) != CAN_PRIO_INFO)  return false;
    if (can_id_type(f->id) != CAN_MSG_HEARTBEAT)  return false;
    if (src == CAN_SRC_GROUND || src == CAN_SRC_BROADCAST) return false;
    if (f->dlc < HEARTBEAT_DLC)                   return false;

    out->source   = src;
    out->state    = f->data[0];
    out->uptime_s = can_get_u32(&f->data[1]);
    return true;
}

typedef struct {
    can_priority_t prio;
    uint8_t        all_type;
    uint8_t        node_type;
} sys_cmd_def_t;

static const sys_cmd_def_t SYS_CMDS[] = {
    [CAN_SYS_UNLOCK] = { CAN_PRIO_IMPORTANT, CAN_CMD_UNLOCK_ALL, CAN_CMD_UNLOCK_NODE },
    [CAN_SYS_SLEEP]  = { CAN_PRIO_IMPORTANT, CAN_CMD_SLEEP_ALL,  CAN_CMD_SLEEP_NODE  },
    [CAN_SYS_RESET]  = { CAN_PRIO_CRITICAL,  CAN_CMD_RESET_ALL,  CAN_CMD_RESET_NODE  },
};

#define SYS_CMD_COUNT (sizeof(SYS_CMDS) / sizeof(SYS_CMDS[0]))

bool can_encode_sys_cmd(can_frame_t *f, can_sys_cmd_t cmd, can_source_t target)
{
    if (cmd <= CAN_SYS_NONE || (size_t)cmd >= SYS_CMD_COUNT) return false;
    const sys_cmd_def_t *d = &SYS_CMDS[cmd];

    if (target == CAN_SRC_BROADCAST) {
        can_frame_init(f, d->prio, CAN_SRC_BROADCAST, d->all_type, 0);
    } else {
        can_frame_init(f, d->prio, CAN_SRC_MASTER, d->node_type, 1);
        f->data[0] = (uint8_t)target;
    }
    return true;
}

can_sys_cmd_t can_decode_sys_cmd(const can_frame_t *f, can_source_t me)
{
    if (f == NULL) return CAN_SYS_NONE;

    for (size_t i = 1; i < SYS_CMD_COUNT; i++) {
        const sys_cmd_def_t *d = &SYS_CMDS[i];

        if (f->id == can_make_id(d->prio, CAN_SRC_BROADCAST, d->all_type)) {
            return (can_sys_cmd_t)i;
        }
        if (f->id == can_make_id(d->prio, CAN_SRC_MASTER, d->node_type) &&
            f->dlc >= 1 && f->data[0] == (uint8_t)me) {
            return (can_sys_cmd_t)i;
        }
    }
    return CAN_SYS_NONE;
}
