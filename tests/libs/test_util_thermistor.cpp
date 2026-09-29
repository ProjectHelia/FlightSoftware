#include <gtest/gtest.h>
#include <cmath>
#include "util_thermistor.h"

static util_ntc_cfg_t cfg(bool ntc_on_top)
{
    return {3300.0f, 10000.0f, 10000.0f, 25.0f, 3950.0f, ntc_on_top};
}

/* Divider voltage the circuit would produce at temp_c (the forward model) */
static float divider_mv(const util_ntc_cfg_t &c, float temp_c)
{
    float r = c.r0_ohm * std::exp(c.beta * (1.0f / (temp_c + 273.15f) - 1.0f / (c.t0_c + 273.15f)));
    return c.ntc_on_top ? c.supply_mv * c.r_fixed_ohm / (r + c.r_fixed_ohm)
                        : c.supply_mv * r / (r + c.r_fixed_ohm);
}

TEST(UtilNtc, MidpointIsReferenceTemperature)
{
    // R_ntc == R_fixed == R0 puts the ADC at half supply in either layout
    util_ntc_cfg_t top = cfg(true), bottom = cfg(false);
    EXPECT_NEAR(util_ntc_temp_c(&top, 1650.0f), 25.0f, 0.01f);
    EXPECT_NEAR(util_ntc_temp_c(&bottom, 1650.0f), 25.0f, 0.01f);
}

TEST(UtilNtc, InvertsTheDividerAcrossTheRange)
{
    for (bool top : {true, false}) {
        util_ntc_cfg_t c = cfg(top);
        for (float t : {-40.0f, -10.0f, 0.0f, 35.0f, 60.0f, 100.0f}) {
            EXPECT_NEAR(util_ntc_temp_c(&c, divider_mv(c, t)), t, 0.05f)
                << "top=" << top << " t=" << t;
        }
    }
}

TEST(UtilNtc, HotterMeansHigherVoltageWhenNtcOnTop)
{
    util_ntc_cfg_t c = cfg(true);
    EXPECT_GT(util_ntc_temp_c(&c, 2000.0f), util_ntc_temp_c(&c, 1500.0f));
}

TEST(UtilNtc, OpenOrShortedThermistorIsNaN)
{
    util_ntc_cfg_t c = cfg(true);
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(&c, 0.0f)));
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(&c, 3300.0f)));
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(&c, -5.0f)));
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(&c, NAN)));
}

TEST(UtilNtc, BadConfigIsNaN)
{
    util_ntc_cfg_t c = cfg(true);
    c.beta = 0.0f;
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(&c, 1650.0f)));
    EXPECT_TRUE(std::isnan(util_ntc_temp_c(nullptr, 1650.0f)));
}