/* Instrumentation HAL for running on a laptop. */
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>

bool hal_init(void) {
    /* The simulated CAN bus is added with the simulator step. */
    return true;
}

void hal_restart(void) {
    printf("[sim] restart requested\n");
    exit(0);
}

hal_sht45_t hal_read_sht45(void) {
    hal_sht45_t r = { 0 };
    r.ok = true;
    r.temp_c_x10 = 250; /* 25.0 C */
    r.rh_pct_x10 = 500; /* 50.0 % */
    return r;
}

hal_ads1115_t hal_read_ads1115(void) {
    hal_ads1115_t r = { 0 };
    r.ok = true;
    r.mv[0] = 1000; /* 1.000 V */
    r.mv[1] = 2000; /* 2.000 V */
    r.mv[2] = 3000; /* 3.000 V */
    r.mv[3] = 4000; /* 4.000 V */
    return r;
}

hal_scd41_t hal_read_scd41(void) {
    hal_scd41_t r = { 0 };
    r.ok = true;
    r.co2_ppm = 400; /* 400 ppm */
    r.temp_c_x10 = 250; /* 25.0 C */
    r.rh_pct_x10 = 500; /* 50.0 % */
    return r;
}

hal_ms5611_t hal_read_ms5611(void) {
    hal_ms5611_t r = { 0 };
    r.ok = true;
    r.pressure_mbar_x10 = 101325; /* 1013.25 mbar */
    r.temp_c_x10 = 250; /* 25.0 C */
    return r;
}

hal_imu_t hal_read_imu(void) {
    hal_imu_t r = { 0 };
    r.ok = true;
    r.ax_mmss = 0; /* 0 mm/s^2 */
    r.ay_mmss = 0; /* 0 mm/s^2 */
    r.az_mmss = 1000; /* 1000 mm/s^2 (1 g) */
    return r;
}
