#include "can_master.h"

#define STATUS_DLC 8u

void can_encode_master_status(can_frame_t *f, const can_master_status_t *s)
{
    can_frame_init(f, CAN_PRIO_INFO, CAN_SRC_MASTER, CAN_MSG_MASTER_STATUS, STATUS_DLC);
    can_put_u32(&f->data[0], s->uptime_s);
    can_put_u16(&f->data[4], s->uplink_cmds);
    f->data[6] = (uint8_t)s->chip_temp_c;
    f->data[7] = s->dl_drops;
}

bool can_decode_master_status(const can_frame_t *f, can_master_status_t *out)
{
    if (f == NULL || out == NULL) return false;
    if (f->id != can_make_id(CAN_PRIO_INFO, CAN_SRC_MASTER, CAN_MSG_MASTER_STATUS)) return false;
    if (f->dlc < STATUS_DLC) return false;

    out->uptime_s    = can_get_u32(&f->data[0]);
    out->uplink_cmds = can_get_u16(&f->data[4]);
    out->chip_temp_c = (int8_t)f->data[6];
    out->dl_drops    = f->data[7];
    return true;
}