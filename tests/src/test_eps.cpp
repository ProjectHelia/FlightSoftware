#include <gtest/gtest.h>
#include <cmath>
#include "control.h"
#include "comms.h"
#include "can_eps.h"
#include "util_math.h"

/* Pin voltage the real divider gives at temp_c (independent of util_ntc_temp_c) */
static float divider_mv(float temp_c)
{
    const util_ntc_cfg_t c = EPS_NTC_CFG;
    float r = c.r0_ohm * std::exp(c.beta * (1.0f / (temp_c + 273.15f) - 1.0f / (c.t0_c + 273.15f)));
    return c.ntc_on_top ? c.supply_mv * c.r_fixed_ohm / (r + c.r_fixed_ohm)
                        : c.supply_mv * r / (r + c.r_fixed_ohm);
}

static const float MV_25C = divider_mv(25.0f);

class EpsControl : public ::testing::Test {
protected:
    eps_control_t ctl;
    eps_data_t d;
    void SetUp() override { eps_control_init(&ctl); }
    void step(bool s_ok, float s_mv, bool t_ok, float t_mv)
    {
        eps_raw_t raw = {s_ok, s_mv, t_ok, t_mv};
        eps_control_step(&ctl, &raw, &d);
    }
};

TEST_F(EpsControl, CurrentFromShunt)
{
    step(true, 41.2f, true, MV_25C);
    EXPECT_EQ(d.shunt_mv_x100, 4120);
    EXPECT_EQ(d.current_ma, 412); // 41.2 mV / 0.1 ohm
}

TEST_F(EpsControl, NegativeCurrentIsValid)
{
    step(true, -1.0f, true, MV_25C);
    EXPECT_EQ(d.current_ma, -10);
}

TEST_F(EpsControl, SaturatedOrMissingShuntIsInvalid)
{
    step(true, 81.92f, true, MV_25C);
    EXPECT_EQ(d.current_ma, UTIL_I16_INVALID);
    EXPECT_EQ(d.shunt_mv_x100, UTIL_I16_INVALID);

    step(false, 10.0f, true, MV_25C);
    EXPECT_EQ(d.current_ma, UTIL_I16_INVALID);
    EXPECT_NE(d.temp_c_x10, UTIL_I16_INVALID); // one bad sensor doesn't hide the other
}

TEST_F(EpsControl, ThermistorAt25C)
{
    step(true, 0.0f, true, MV_25C);
    EXPECT_EQ(d.temp_c_x10, 250);
}

TEST_F(EpsControl, ThermistorColdAndWarm)
{
    step(true, 0.0f, true, divider_mv(-20.0f));
    EXPECT_EQ(d.temp_c_x10, -200);

    eps_control_init(&ctl); // fresh filter so the next reading isn't smoothed
    step(true, 0.0f, true, divider_mv(30.0f));
    EXPECT_EQ(d.temp_c_x10, 300);
}

TEST_F(EpsControl, ClippedAdcIsInvalidNotAFalseTemperature)
{
    step(true, 0.0f, true, 3100.0f);
    EXPECT_EQ(d.temp_c_x10, UTIL_I16_INVALID);
}

TEST_F(EpsControl, DisconnectedThermistorIsInvalid)
{
    step(true, 0.0f, true, 0.0f);      // pin pulled to ground
    EXPECT_EQ(d.temp_c_x10, UTIL_I16_INVALID);
    step(true, 0.0f, false, MV_25C);  // ADC read failed
    EXPECT_EQ(d.temp_c_x10, UTIL_I16_INVALID);
}

TEST_F(EpsControl, TemperatureSpikeIsSmoothed)
{
    step(true, 0.0f, true, MV_25C);             // 25.0 degC seeds the filter
    step(true, 0.0f, true, divider_mv(30.0f));  // sudden jump to 30 degC
    EXPECT_EQ(d.temp_c_x10, 260);               // only 20% of the way there
}

class EpsComms : public ::testing::Test {
protected:
    eps_comms_t c;
    eps_data_t data = {412, 4120, 301};
    can_frame_t tx[EPS_COMMS_MAX_TX];

    void SetUp() override
    {
        eps_comms_init(&c, 0);
        eps_comms_boot_done(&c); // SAFE, as setup.c does
    }
    void receive(can_sys_cmd_t cmd, can_source_t target)
    {
        can_frame_t f;
        can_encode_sys_cmd(&f, cmd, target);
        eps_comms_on_frame(&c, &f);
    }
};

TEST_F(EpsComms, SafeSendsHeartbeatButNoStatus)
{
    ASSERT_EQ(eps_comms_on_tick(&c, 0, &data, tx), 1u);
    can_heartbeat_t hb;
    ASSERT_TRUE(can_decode_heartbeat(&tx[0], &hb));
    EXPECT_EQ(hb.source, CAN_SRC_EPS);
    EXPECT_EQ(hb.state, FSM_SAFE);
}

TEST_F(EpsComms, UnlockStartsStatus)
{
    receive(CAN_SYS_UNLOCK, CAN_SRC_EPS);
    EXPECT_EQ(c.state, FSM_ACTIVE);

    ASSERT_EQ(eps_comms_on_tick(&c, 0, &data, tx), 2u); // heartbeat + status
    can_eps_status_t s;
    ASSERT_TRUE(can_decode_eps_status(&tx[1], &s));
    EXPECT_EQ(s.current_ma, 412);
    EXPECT_EQ(s.shunt_mv_x100, 4120);
    EXPECT_EQ(s.temp_c_x10, 301);
}

TEST_F(EpsComms, StatusEverySecond)
{
    receive(CAN_SYS_UNLOCK, CAN_SRC_BROADCAST);
    EXPECT_EQ(eps_comms_on_tick(&c, 0, &data, tx), 2u);
    EXPECT_EQ(eps_comms_on_tick(&c, 500, &data, tx), 1u);  // heartbeat only
    EXPECT_EQ(eps_comms_on_tick(&c, 1000, &data, tx), 2u); // both
}

TEST_F(EpsComms, OtherNodesUnlockIsIgnored)
{
    receive(CAN_SYS_UNLOCK, CAN_SRC_THERMAL);
    EXPECT_EQ(c.state, FSM_SAFE);
}

TEST_F(EpsComms, SleepAndReset)
{
    receive(CAN_SYS_RESET, CAN_SRC_BROADCAST);
    EXPECT_TRUE(c.reset_requested);
    receive(CAN_SYS_SLEEP, CAN_SRC_EPS);
    EXPECT_EQ(c.state, FSM_SLEEP);
}

TEST_F(EpsComms, HeartbeatEvery500ms)
{
    EXPECT_EQ(eps_comms_on_tick(&c, 0, &data, tx), 1u);
    EXPECT_EQ(eps_comms_on_tick(&c, 499, &data, tx), 0u);
    EXPECT_EQ(eps_comms_on_tick(&c, 500, &data, tx), 1u);
}

TEST_F(EpsComms, TimingSurvivesClockWrap)
{
    eps_comms_init(&c, UINT32_MAX - 100);
    EXPECT_EQ(eps_comms_on_tick(&c, UINT32_MAX - 100, &data, tx), 1u);
    EXPECT_EQ(eps_comms_on_tick(&c, 398, &data, tx), 0u); // 499 ms later, wrapped
    EXPECT_EQ(eps_comms_on_tick(&c, 399, &data, tx), 1u); // 500 ms later
}