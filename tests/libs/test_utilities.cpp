#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "util_math.h"
#include "util_filter.h"
#include "util_control.h"
#include "util_timer.h"

static const float kNaN = std::numeric_limits<float>::quiet_NaN();
static const float kInf = std::numeric_limits<float>::infinity();

TEST(UtilMath, InRange) {
    EXPECT_TRUE(util_in_range(37.2f, -50.0f, 100.0f));
    EXPECT_TRUE(util_in_range(-50.0f, -50.0f, 100.0f));   // edges inclusive
    EXPECT_FALSE(util_in_range(-200.0f, -50.0f, 100.0f)); // the SED's example
    EXPECT_FALSE(util_in_range(kNaN, -50.0f, 100.0f));
    EXPECT_FALSE(util_in_range(kInf, -50.0f, 100.0f));
}

TEST(UtilMath, Clamp) {
    EXPECT_EQ(util_clampf(5.0f, 0.0f, 10.0f), 5.0f);
    EXPECT_EQ(util_clampf(-1.0f, 0.0f, 10.0f), 0.0f);
    EXPECT_EQ(util_clampf(11.0f, 0.0f, 10.0f), 10.0f);
    EXPECT_EQ(util_clampf(kNaN, 0.0f, 10.0f), 0.0f);
}

TEST(UtilMath, ToI16RoundsAndScales) {
    EXPECT_EQ(util_to_i16(37.2f, 10.0f), 372);       // SED example
    EXPECT_EQ(util_to_i16(-12.36f, 10.0f), -124);    // rounds to nearest
    EXPECT_EQ(util_to_i16(28.012f, 1000.0f), 28012); // volts -> mV
}

TEST(UtilMath, ToI16SaturatesAndFlagsInvalid) {
    EXPECT_EQ(util_to_i16(1e9f, 1.0f), INT16_MAX);
    EXPECT_EQ(util_to_i16(-1e9f, 1.0f), INT16_MIN + 1); // never collides with INVALID
    EXPECT_EQ(util_to_i16(kInf, 1.0f), INT16_MAX);
    EXPECT_EQ(util_to_i16(kNaN, 10.0f), UTIL_I16_INVALID);
}

TEST(UtilMath, FromI16) {
    EXPECT_FLOAT_EQ(util_from_i16(372, 10.0f), 37.2f);
    EXPECT_TRUE(std::isnan(util_from_i16(UTIL_I16_INVALID, 10.0f)));
    EXPECT_TRUE(std::isnan(util_from_i16(100, 0.0f)));
}

TEST(UtilEma, FirstSampleSeedsOutput) {
    util_ema_t f;
    util_ema_init(&f, 0.1f);
    EXPECT_FLOAT_EQ(util_ema_update(&f, 35.0f), 35.0f); // no ramp from 0
}

TEST(UtilEma, ConvergesToStepInput) {
    util_ema_t f;
    util_ema_init(&f, 0.2f);
    util_ema_update(&f, 0.0f);
    float out = 0.0f;
    for (int i = 0; i < 100; i++)
        out = util_ema_update(&f, 10.0f);
    EXPECT_NEAR(out, 10.0f, 1e-3f);
}

TEST(UtilEma, SmoothsASpike) {
    util_ema_t f;
    util_ema_init(&f, 0.1f);
    util_ema_update(&f, 35.0f);
    EXPECT_NEAR(util_ema_update(&f, 135.0f), 45.0f, 1e-4f); // 10% of the jump
}

TEST(UtilEma, BadAlphaMeansNoFiltering) {
    for (float a : { 0.0f, -0.5f, 1.5f, kNaN }) {
        util_ema_t f;
        util_ema_init(&f, a);
        util_ema_update(&f, 1.0f);
        EXPECT_FLOAT_EQ(util_ema_update(&f, 7.0f), 7.0f) << "alpha=" << a;
    }
}

TEST(UtilBangBang, HysteresisCycle) {
    util_bangbang_t c;
    util_bangbang_init(&c, 35.0f, 0.5f);
    EXPECT_FALSE(c.on);

    EXPECT_TRUE(util_bangbang_update(&c, 30.0f));  // cold: on
    EXPECT_TRUE(util_bangbang_update(&c, 35.2f));  // in band while heating: stays on
    EXPECT_FALSE(util_bangbang_update(&c, 35.5f)); // top of band: off
    EXPECT_FALSE(util_bangbang_update(&c, 34.8f)); // in band while cooling: stays off
    EXPECT_TRUE(util_bangbang_update(&c, 34.5f));  // bottom of band: on
}

TEST(UtilBangBang, NaNTurnsOff) {
    util_bangbang_t c;
    util_bangbang_init(&c, 35.0f, 0.5f);
    util_bangbang_update(&c, 20.0f);
    EXPECT_FALSE(util_bangbang_update(&c, kNaN));
}

TEST(UtilBangBang, SetpointChangeTakesEffect) {
    util_bangbang_t c;
    util_bangbang_init(&c, 35.0f, 0.5f);
    EXPECT_FALSE(util_bangbang_update(&c, 36.0f));
    c.setpoint = 40.0f; // e.g. CMD_SET_TARGET between flight phases
    EXPECT_TRUE(util_bangbang_update(&c, 36.0f));
}

TEST(UtilTimer, FiresOncePerPeriod) {
    uint32_t next = 0;
    EXPECT_TRUE(util_every(0, &next, 500)); // first call fires immediately
    EXPECT_FALSE(util_every(499, &next, 500));
    EXPECT_TRUE(util_every(500, &next, 500));
    EXPECT_FALSE(util_every(999, &next, 500));
}

TEST(UtilTimer, SurvivesClockWrap) {
    uint32_t next = UINT32_MAX - 100;
    EXPECT_TRUE(util_every(UINT32_MAX - 100, &next, 500));
    EXPECT_FALSE(util_every(398, &next, 500)); // 499 ms later, wrapped
    EXPECT_TRUE(util_every(399, &next, 500));  // 500 ms later
}