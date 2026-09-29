#include <gtest/gtest.h>
#include "can_eps.h"

TEST(CanEps, StatusRoundTrip)
{
    can_eps_status_t in = {412, 4120, -35}, out = {};
    can_frame_t f;
    can_encode_eps_status(&f, &in);
    EXPECT_EQ(f.dlc, 6);
    ASSERT_TRUE(can_decode_eps_status(&f, &out));
    EXPECT_EQ(out.current_ma, 412);
    EXPECT_EQ(out.shunt_mv_x100, 4120);
    EXPECT_EQ(out.temp_c_x10, -35);
}

TEST(CanEps, InvalidSentinelSurvives)
{
    can_eps_status_t in = {INT16_MIN, INT16_MIN, INT16_MIN}, out = {};
    can_frame_t f;
    can_encode_eps_status(&f, &in);
    ASSERT_TRUE(can_decode_eps_status(&f, &out));
    EXPECT_EQ(out.current_ma, INT16_MIN);
    EXPECT_EQ(out.temp_c_x10, INT16_MIN);
}

TEST(CanEps, RejectsOtherFrames)
{
    can_eps_status_t out;
    can_frame_t f;
    can_heartbeat_t hb = {CAN_SRC_EPS, 1, 0};
    can_encode_heartbeat(&f, &hb);
    EXPECT_FALSE(can_decode_eps_status(&f, &out));

    can_eps_status_t in = {1, 2, 3};
    can_encode_eps_status(&f, &in);
    f.dlc = 5;
    EXPECT_FALSE(can_decode_eps_status(&f, &out));
    EXPECT_FALSE(can_decode_eps_status(nullptr, &out));
}