#include "can_instr.h"
#include "protocol.h"

/* Little-endian packing helpers, explicit rather than memcpy'ing the
 * struct - avoids relying on host/target sharing endianness or struct
 * layout, same approach as can_master.c. */

static void put_i16(uint8_t *p, int16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF); }
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF); }
static void put_i32(uint8_t *p, int32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static int16_t  get_i16(const uint8_t *p) { return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static int32_t  get_i32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void make_frame(can_frame_t *f, uint32_t type, uint8_t dlc)
{
    f->id  = can_make_id(CAN_PRIO_INFO, CAN_SRC_INSTRUMENTATION, type);
    f->dlc = dlc;
}

static bool frame_is(const can_frame_t *f, uint32_t type, uint8_t min_dlc)
{
    return can_id_type(f->id) == type && f->dlc >= min_dlc;
}

void can_encode_pressure(can_frame_t *f, const can_instr_pressure_t *s)
{
    make_frame(f, CAN_MSG_PRESSURE, 4);
    put_i32(&f->data[0], s->pressure_mbar_x10);
}

bool can_decode_pressure(const can_frame_t *f, can_instr_pressure_t *out)
{
    if (!frame_is(f, CAN_MSG_PRESSURE, 4)) return false;
    out->pressure_mbar_x10 = get_i32(&f->data[0]);
    return true;
}

void can_encode_altitude(can_frame_t *f, const can_instr_altitude_t *s)
{
    make_frame(f, CAN_MSG_ALTITUDE, 4);
    put_i32(&f->data[0], s->altitude_m_x10);
}

bool can_decode_altitude(const can_frame_t *f, can_instr_altitude_t *out)
{
    if (!frame_is(f, CAN_MSG_ALTITUDE, 4)) return false;
    out->altitude_m_x10 = get_i32(&f->data[0]);
    return true;
}

void can_encode_housekeeping(can_frame_t *f, const can_instr_housekeeping_t *s)
{
    make_frame(f, CAN_MSG_HOUSEKEEPING, 8);
    put_i32(&f->data[0], s->pressure_mbar_x10);
    put_i32(&f->data[4], s->altitude_m_x10);
}

bool can_decode_housekeeping(const can_frame_t *f, can_instr_housekeeping_t *out)
{
    if (!frame_is(f, CAN_MSG_HOUSEKEEPING, 8)) return false;
    out->pressure_mbar_x10 = get_i32(&f->data[0]);
    out->altitude_m_x10    = get_i32(&f->data[4]);
    return true;
}

void can_encode_humidity(can_frame_t *f, const can_instr_humidity_t *s)
{
    make_frame(f, CAN_MSG_HUMIDITY, 4);
    put_i16(&f->data[0], s->temp_c_x10);
    put_i16(&f->data[2], s->rh_pct_x10);
}

bool can_decode_humidity(const can_frame_t *f, can_instr_humidity_t *out)
{
    if (!frame_is(f, CAN_MSG_HUMIDITY, 4)) return false;
    out->temp_c_x10 = get_i16(&f->data[0]);
    out->rh_pct_x10 = get_i16(&f->data[2]);
    return true;
}

void can_encode_gas(can_frame_t *f, const can_instr_gas_t *s)
{
    make_frame(f, CAN_MSG_GAS, 6);
    put_u16(&f->data[0], s->co2_ppm);
    put_i16(&f->data[2], s->temp_c_x10);
    put_i16(&f->data[4], s->rh_pct_x10);
}

bool can_decode_gas(const can_frame_t *f, can_instr_gas_t *out)
{
    if (!frame_is(f, CAN_MSG_GAS, 6)) return false;
    out->co2_ppm    = get_u16(&f->data[0]);
    out->temp_c_x10 = get_i16(&f->data[2]);
    out->rh_pct_x10 = get_i16(&f->data[4]);
    return true;
}

void can_encode_accel(can_frame_t *f, const can_instr_accel_t *s)
{
    make_frame(f, CAN_MSG_ACCEL, 6);
    put_i16(&f->data[0], s->ax_mmss);
    put_i16(&f->data[2], s->ay_mmss);
    put_i16(&f->data[4], s->az_mmss);
}

bool can_decode_accel(const can_frame_t *f, can_instr_accel_t *out)
{
    if (!frame_is(f, CAN_MSG_ACCEL, 6)) return false;
    out->ax_mmss = get_i16(&f->data[0]);
    out->ay_mmss = get_i16(&f->data[2]);
    out->az_mmss = get_i16(&f->data[4]);
    return true;
}

void can_encode_status(can_frame_t *f, const can_instr_status_t *s)
{
    make_frame(f, CAN_MSG_STATUS, 3);
    f->data[0] = s->sensor_status;
    put_i16(&f->data[1], s->board_temp_c_x10);
}

bool can_decode_status(const can_frame_t *f, can_instr_status_t *out)
{
    if (!frame_is(f, CAN_MSG_STATUS, 3)) return false;
    out->sensor_status    = f->data[0];
    out->board_temp_c_x10 = get_i16(&f->data[1]);
    return true;
}