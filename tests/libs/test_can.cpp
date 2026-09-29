#include <gtest/gtest.h>
#include <cstring>
#include "can.h"

TEST(CanId, RoundTripsEveryCombination) {
    for (unsigned p = 0; p < 4; p++)
        for (unsigned s = 0; s < 8; s++)
            for (unsigned t = 0; t < 64; t++) {
                uint32_t id = can_make_id((can_priority_t)p, (can_source_t)s, (uint8_t)t);
                ASSERT_LE(id, 0x7FFu);
                ASSERT_EQ(can_id_priority(id), (can_priority_t)p);
                ASSERT_EQ(can_id_source(id), (can_source_t)s);
                ASSERT_EQ(can_id_type(id), t);
            }
}

TEST(CanId, OutOfRangeTypeIsMaskedNotSpilled) {
    // 0x41 is 7 bits; must not corrupt the source field
    uint32_t id = can_make_id(CAN_PRIO_INFO, CAN_SRC_EPS, 0x41);
    EXPECT_EQ(can_id_source(id), CAN_SRC_EPS);
    EXPECT_EQ(can_id_type(id), 0x01);
}

TEST(CanId, KnownValues) {
    EXPECT_EQ(can_make_id(CAN_PRIO_INFO, CAN_SRC_MASTER, CAN_MSG_HEARTBEAT), 0x640u);
    EXPECT_EQ(can_make_id(CAN_PRIO_IMPORTANT, CAN_SRC_MASTER, CAN_CMD_UNLOCK_NODE), 0x240u);
}

TEST(CanBytes, LittleEndianLayout) {
    uint8_t b[4] = {};
    can_put_u32(b, 0x11223344u);
    EXPECT_EQ(b[0], 0x44);
    EXPECT_EQ(b[1], 0x33);
    EXPECT_EQ(b[2], 0x22);
    EXPECT_EQ(b[3], 0x11);
    can_put_u16(b, 0xABCD);
    EXPECT_EQ(b[0], 0xCD);
    EXPECT_EQ(b[1], 0xAB);
}

TEST(CanBytes, RoundTripsExtremes) {
    uint8_t b[4] = {};
    const int16_t i16s[] = { INT16_MIN, -1, 0, 372, INT16_MAX };
    for (int16_t v : i16s) {
        can_put_i16(b, v);
        EXPECT_EQ(can_get_i16(b), v);
    }
    for (uint32_t v : { 0u, 1u, 0xDEADBEEFu, UINT32_MAX }) {
        can_put_u32(b, v);
        EXPECT_EQ(can_get_u32(b), v);
    }
}

TEST(CanFrame, InitZeroesPayloadAndClampsDlc) {
    can_frame_t f;
    memset(&f, 0xFF, sizeof f);
    can_frame_init(&f, CAN_PRIO_INFO, CAN_SRC_EPS, 0x04, 12);
    EXPECT_EQ(f.dlc, 8);
    for (uint8_t byte : f.data)
        EXPECT_EQ(byte, 0);
}

/* ---- Heartbeat --------------------------------------------------------- */

TEST(CanHeartbeat, RoundTrip) {
    can_heartbeat_t in = { CAN_SRC_EPS, 2, 123456u }, out = {};
    can_frame_t f;
    can_encode_heartbeat(&f, &in);
    ASSERT_TRUE(can_decode_heartbeat(&f, &out));
    EXPECT_EQ(out.source, CAN_SRC_EPS);
    EXPECT_EQ(out.state, 2);
    EXPECT_EQ(out.uptime_s, 123456u);
}

TEST(CanHeartbeat, RejectsCommandsAndShortFrames) {
    can_heartbeat_t out = {};
    can_frame_t f;

    can_encode_sys_cmd(&f, CAN_SYS_UNLOCK, CAN_SRC_BROADCAST); // type 0x00 too
    EXPECT_FALSE(can_decode_heartbeat(&f, &out));

    can_encode_sys_cmd(&f, CAN_SYS_UNLOCK, CAN_SRC_EPS); // Master, type 0x00
    EXPECT_FALSE(can_decode_heartbeat(&f, &out));

    can_heartbeat_t hb = { CAN_SRC_EPS, 1, 5 };
    can_encode_heartbeat(&f, &hb);
    f.dlc = 4;
    EXPECT_FALSE(can_decode_heartbeat(&f, &out));
    EXPECT_FALSE(can_decode_heartbeat(nullptr, &out));
}

/* ---- System commands --------------------------------------------------- */

TEST(CanSysCmd, MasterHeartbeatIsNotAnUnlock) {
    // The old bug: Master heartbeat and CMD_UNLOCK_NODE share source 0x1 and
    // type 0x00. A heartbeat whose first byte equals our ID must not unlock us.
    can_heartbeat_t hb = { CAN_SRC_MASTER, (uint8_t)CAN_SRC_EPS, 0 };
    can_frame_t f;
    can_encode_heartbeat(&f, &hb);
    ASSERT_EQ(f.data[0], (uint8_t)CAN_SRC_EPS);
    EXPECT_EQ(can_decode_sys_cmd(&f, CAN_SRC_EPS), CAN_SYS_NONE);
}

struct SysCase {
    const char *name;
    can_sys_cmd_t sent;
    can_source_t target;
    can_source_t me;
    can_sys_cmd_t expected;
};

class CanSysCmdTable : public ::testing::TestWithParam<SysCase> { };

TEST_P(CanSysCmdTable, EncodeThenDecode) {
    const SysCase &c = GetParam();
    can_frame_t f;
    ASSERT_TRUE(can_encode_sys_cmd(&f, c.sent, c.target));
    EXPECT_EQ(can_decode_sys_cmd(&f, c.me), c.expected);
}

INSTANTIATE_TEST_SUITE_P(All, CanSysCmdTable, ::testing::Values(SysCase { "UnlockAll", CAN_SYS_UNLOCK, CAN_SRC_BROADCAST, CAN_SRC_EPS, CAN_SYS_UNLOCK }, SysCase { "SleepAll", CAN_SYS_SLEEP, CAN_SRC_BROADCAST, CAN_SRC_EPS, CAN_SYS_SLEEP }, SysCase { "ResetAll", CAN_SYS_RESET, CAN_SRC_BROADCAST, CAN_SRC_EPS, CAN_SYS_RESET }, SysCase { "UnlockMe", CAN_SYS_UNLOCK, CAN_SRC_EPS, CAN_SRC_EPS, CAN_SYS_UNLOCK }, SysCase { "SleepMe", CAN_SYS_SLEEP, CAN_SRC_EPS, CAN_SRC_EPS, CAN_SYS_SLEEP }, SysCase { "ResetMe", CAN_SYS_RESET, CAN_SRC_EPS, CAN_SRC_EPS, CAN_SYS_RESET }, SysCase { "UnlockOtherNode", CAN_SYS_UNLOCK, CAN_SRC_THERMAL, CAN_SRC_EPS, CAN_SYS_NONE }, SysCase { "ResetOtherNode", CAN_SYS_RESET, CAN_SRC_THERMAL, CAN_SRC_EPS, CAN_SYS_NONE }), [](const auto &info) { return std::string(info.param.name); });

TEST(CanSysCmd, TargetedCommandWithNoPayloadIsIgnored) {
    can_frame_t f;
    can_encode_sys_cmd(&f, CAN_SYS_UNLOCK, CAN_SRC_EPS);
    f.dlc = 0; // data[0] still holds our ID, but it isn't part of the frame
    EXPECT_EQ(can_decode_sys_cmd(&f, CAN_SRC_EPS), CAN_SYS_NONE);
}

TEST(CanSysCmd, WrongPriorityIsIgnored) {
    can_frame_t f;
    can_frame_init(&f, CAN_PRIO_INFO, CAN_SRC_BROADCAST, CAN_CMD_UNLOCK_ALL, 0);
    EXPECT_EQ(can_decode_sys_cmd(&f, CAN_SRC_EPS), CAN_SYS_NONE);
}

TEST(CanSysCmd, BadInputs) {
    can_frame_t f;
    EXPECT_FALSE(can_encode_sys_cmd(&f, CAN_SYS_NONE, CAN_SRC_EPS));
    EXPECT_FALSE(can_encode_sys_cmd(&f, (can_sys_cmd_t)99, CAN_SRC_EPS));
    EXPECT_EQ(can_decode_sys_cmd(nullptr, CAN_SRC_EPS), CAN_SYS_NONE);
}