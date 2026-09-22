#include "can_eps.h"

#define STATUS_DLC 6u

void can_encode_eps_status(can_frame_t *f, const can_eps_status_t *s)
{
    can_frame_init(f, CAN_PRIO_INFO, CAN_SRC_EPS, CAN_EPS_STATUS, STATUS_DLC);
    can_put_i16(&f->data[0], s->current_ma);
    can_put_i16(&f->data[2], s->shunt_mv_x100);
    can_put_i16(&f->data[4], s->temp_c_x10);
}

bool can_decode_eps_status(const can_frame_t *f, can_eps_status_t *out)
{
    if (f == NULL || out == NULL) return false;
    if (f->id != can_make_id(CAN_PRIO_INFO, CAN_SRC_EPS, CAN_EPS_STATUS)) return false;
    if (f->dlc < STATUS_DLC) return false;

    out->current_ma    = can_get_i16(&f->data[0]);
    out->shunt_mv_x100 = can_get_i16(&f->data[2]);
    out->temp_c_x10    = can_get_i16(&f->data[4]);
    return true;
}