#include <gtest/gtest.h>
#include <string>
#include "comms.h"
#include "can_eps.h"

class MasterComms : public ::testing::Test {
protected:
    master_comms_t c;
    can_frame_t tx[MASTER_COMMS_MAX_TX];
    char text[96];

    void SetUp() override { master_comms_init(&c, 0); }

    can_frame_t heartbeat_from(can_source_t src, fsm_state_t state)
    {
        can_frame_t f;
        can_heartbeat_t hb = {src, (uint8_t)state, 12};
        can_encode_heartbeat(&f, &hb);
        return f;
    }
};

TEST_F(MasterComms, StartsActiveAndSendsHeartbeat)
{
    EXPECT_EQ(c.state, FSM_ACTIVE);
    ASSERT_EQ(master_comms_on_tick(&c, 0, tx), 1u);
    can_heartbeat_t hb;
    ASSERT_TRUE(can_decode_heartbeat(&tx[0], &hb));
    EXPECT_EQ(hb.source, CAN_SRC_MASTER);
    EXPECT_EQ(hb.state, FSM_ACTIVE);
    EXPECT_EQ(master_comms_on_tick(&c, 499, tx), 0u);
    EXPECT_EQ(master_comms_on_tick(&c, 500, tx), 1u);
}

TEST_F(MasterComms, UnlocksANodeInSafe)
{
    can_frame_t hb = heartbeat_from(CAN_SRC_EPS, FSM_SAFE);
    ASSERT_EQ(master_comms_on_frame(&c, 0, &hb, tx), 1u);
    // The reply is an unlock EPS accepts, and no other node does
    EXPECT_EQ(can_decode_sys_cmd(&tx[0], CAN_SRC_EPS), CAN_SYS_UNLOCK);
    EXPECT_EQ(can_decode_sys_cmd(&tx[0], CAN_SRC_THERMAL), CAN_SYS_NONE);
}

TEST_F(MasterComms, LeavesActiveNodesAlone)
{
    can_frame_t hb = heartbeat_from(CAN_SRC_EPS, FSM_ACTIVE);
    EXPECT_EQ(master_comms_on_frame(&c, 0, &hb, tx), 0u);
}

TEST_F(MasterComms, IgnoresOtherFrames)
{
    can_frame_t f;
    can_eps_status_t s = {412, 4120, 301};
    can_encode_eps_status(&f, &s);
    EXPECT_EQ(master_comms_on_frame(&c, 0, &f, tx), 0u);
}

TEST_F(MasterComms, DescribesHeartbeat)
{
    can_frame_t hb = heartbeat_from(CAN_SRC_EPS, FSM_SAFE);
    EXPECT_EQ(std::string(master_describe_frame(&hb, text, sizeof text)),
              "EPS heartbeat: SAFE, up 12 s");
}

TEST_F(MasterComms, DescribesEpsStatusIncludingInvalid)
{
    can_frame_t f;
    can_eps_status_t s = {412, 4120, INT16_MIN};
    can_encode_eps_status(&f, &s);
    EXPECT_EQ(std::string(master_describe_frame(&f, text, sizeof text)),
              "EPS status: current 412.00 mA, shunt 41.20 mV, PCB invalid");
}

TEST_F(MasterComms, DescribesUnknownFrames)
{
    can_frame_t f;
    can_frame_init(&f, CAN_PRIO_WARNING, CAN_SRC_THERMAL, 0x06, 2);
    EXPECT_EQ(std::string(master_describe_frame(&f, text, sizeof text)),
              "THERMAL frame: prio 2, type 0x06, 2 bytes (id 0x5C6)");
}

TEST_F(MasterComms, DescribeNeverOverflows)
{
    can_frame_t hb = heartbeat_from(CAN_SRC_INSTRUMENTATION, FSM_SAFE);
    char tiny[8];
    master_describe_frame(&hb, tiny, sizeof tiny);
    EXPECT_EQ(std::string(tiny).size(), 7u);
}

TEST_F(MasterComms, TracksNodesComingAndGoing)
{
    EXPECT_EQ(c.alive_mask, 0);
    can_frame_t eps = heartbeat_from(CAN_SRC_EPS, FSM_ACTIVE);
    can_frame_t ins = heartbeat_from(CAN_SRC_INSTRUMENTATION, FSM_ACTIVE);
    master_comms_on_frame(&c, 1000, &eps, tx);
    master_comms_on_frame(&c, 1000, &ins, tx);
    EXPECT_EQ(c.alive_mask, (1u << CAN_SRC_EPS) | (1u << CAN_SRC_INSTRUMENTATION));

    master_comms_on_frame(&c, 2400, &eps, tx);  // EPS keeps talking, Instrumentation goes quiet
    master_comms_on_tick(&c, 2500, tx);         // 1500 ms since Instrumentation: not yet lost
    EXPECT_EQ(c.alive_mask, (1u << CAN_SRC_EPS) | (1u << CAN_SRC_INSTRUMENTATION));
    master_comms_on_tick(&c, 2501, tx);         // 1501 ms: lost
    EXPECT_EQ(c.alive_mask, 1u << CAN_SRC_EPS);
}

TEST_F(MasterComms, IgnoresItsOwnHeartbeat)
{
    can_frame_t own = heartbeat_from(CAN_SRC_MASTER, FSM_ACTIVE);
    EXPECT_EQ(master_comms_on_frame(&c, 0, &own, tx), 0u);
    EXPECT_EQ(c.alive_mask, 0);
}