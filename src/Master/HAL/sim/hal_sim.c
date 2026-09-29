/* Master HAL for running on ma laptop */
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>

bool hal_init(void) {
    return true;
}

void hal_restart(void) {
    printf("[sim] restart requested\n");
    exit(0);
}