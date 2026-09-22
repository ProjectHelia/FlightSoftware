#include "can_photonics.h"
#include "protocol.h"

/* Little-endian packing helpers - same approach as can_instr.c/can_master.c. */

static void put_i16(uint8_t *p, int16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF); }
static void put_i32(uint8_t *p, int32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static int16_t get_i16(const uint8_t *p) { return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static int32_t get_i32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void make_frame(can_frame_t *f, uint32_t type, uint8_t dlc)
{
    f->id  = can_make_id(CAN_PRIO_INFO, CAN_SRC_PHOTONICS, type);
    f->dlc = dlc;
}

static bool frame_is(const can_frame_t *f, uint32_t type, uint8_t min_dlc)
{
    return can_id_type(f->id) == type && f->dlc >= min_dlc;
}

void can_encode_uv0(can_frame_t *f, const can_photonics_uv_t *s)
{
    make_frame(f, CAN_MSG_UV0, 8);
    put_i32(&f->data[0], s->lux_x10);
    put_i32(&f->data[4], s->uvi_x1000);
}

bool can_decode_uv0(const can_frame_t *f, can_photonics_uv_t *out)
{
    if (!frame_is(f, CAN_MSG_UV0, 8)) return false;
    out->lux_x10   = get_i32(&f->data[0]);
    out->uvi_x1000 = get_i32(&f->data[4]);
    return true;
}

void can_encode_uv1(can_frame_t *f, const can_photonics_uv_t *s)
{
    make_frame(f, CAN_MSG_UV1, 8);
    put_i32(&f->data[0], s->lux_x10);
    put_i32(&f->data[4], s->uvi_x1000);
}

bool can_decode_uv1(const can_frame_t *f, can_photonics_uv_t *out)
{
    if (!frame_is(f, CAN_MSG_UV1, 8)) return false;
    out->lux_x10   = get_i32(&f->data[0]);
    out->uvi_x1000 = get_i32(&f->data[4]);
    return true;
}

void can_encode_adc(can_frame_t *f, const can_photonics_adc_t *s)
{
    make_frame(f, CAN_MSG_ADC, 8);
    put_i16(&f->data[0], s->mv[0]);
    put_i16(&f->data[2], s->mv[1]);
    put_i16(&f->data[4], s->mv[2]);
    put_i16(&f->data[6], s->mv[3]);
}

bool can_decode_adc(const can_frame_t *f, can_photonics_adc_t *out)
{
    if (!frame_is(f, CAN_MSG_ADC, 8)) return false;
    out->mv[0] = get_i16(&f->data[0]);
    out->mv[1] = get_i16(&f->data[2]);
    out->mv[2] = get_i16(&f->data[4]);
    out->mv[3] = get_i16(&f->data[6]);
    return true;
}