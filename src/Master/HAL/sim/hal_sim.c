/* Master HAL for running on a laptop. */
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