/**
 * @file hal.h
 * @brief Master hardware interface: CAN bus and TT&C Ethernet. SD cards and
 *        RTC are added when storage is built out.
 */
#ifndef HELIA_MASTER_HAL_H
#define HELIA_MASTER_HAL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up the CAN bus and the TT&C Ethernet link.
 * @return False only if CAN failed - that's fatal, Master is useless
 *         without the bus. A failed Ethernet bring-up is logged but not
 *         fatal: Master must keep running CAN/flight logic with no ground
 *         link, since link loss is an expected degraded state (the flight
 *         manager holds phase until reconnection), not a reason to halt.
 */
bool hal_init(void);

/** @brief Reboot the node. Does not return. */
void hal_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_HAL_H */