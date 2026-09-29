/**
 * @file control.h
 * @brief Photonics control core: handles most of the data collection and can transmission, heartbeat and shit is in comms.h
 */
#ifndef HELIA_PHOTONICS_CONTROL_H
#define HELIA_PHOTONICS_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Main control task for data collection for photonics. Args are unused, just added this here to avoid compiler warnings!
/// @param arg 
void photonics_control_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_PHOTONICS_CONTROL_H */