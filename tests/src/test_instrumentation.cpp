#include <gtest/gtest.h>
#include "comms.h"

TEST(InstrComms, HeartbeatAsInstrumentation)
{
    instr_comms_t c;
    can_frame_t tx[INSTR_COMMS_MAX_TX];
    instr_comms_init(&c, 0);
    instr_comms_boot_done(&c);

    ASSERT_EQ(instr_comms_on_tick(&c, 0, tx), 1u);
    can_heartbeat_t hb;
    ASSERT_TRUE(can_decode_heartbeat(&tx[0], &hb));
    EXPECT_EQ(hb.source, CAN_SRC_INSTRUMENTATION);
    EXPECT_EQ(hb.state, FSM_SAFE);
}

TEST(InstrComms, UnlockedByMaster)
{
    instr_comms_t c;
    can_frame_t f;
    instr_comms_init(&c, 0);
    instr_comms_boot_done(&c);

    can_encode_sys_cmd(&f, CAN_SYS_UNLOCK, CAN_SRC_EPS); // someone else's unlock
    instr_comms_on_frame(&c, &f);
    EXPECT_EQ(c.state, FSM_SAFE);

    can_encode_sys_cmd(&f, CAN_SYS_UNLOCK, CAN_SRC_INSTRUMENTATION);
    instr_comms_on_frame(&c, &f);
    EXPECT_EQ(c.state, FSM_ACTIVE);
}