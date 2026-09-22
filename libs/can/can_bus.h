/**
 * @file can_bus.h
 * @brief Send and receive CAN frames. One interface, one implementation per
 *        platform: esp32/ (TWAI driver) now, sim/ (fake bus) later.
 *
 * Helpers only: no tasks. The node's comms task is the only caller, so
 * there is exactly one reader of the bus.
 */
#ifndef HELIA_CAN_BUS_H
#define HELIA_CAN_BUS_H

#include <stdbool.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Start the bus at CAN_BITRATE. Pins are ignored by the sim. */
bool can_bus_init(int tx_gpio, int rx_gpio);

/** @brief Queue a frame for sending. False on timeout or bus error. */
bool can_bus_send(const can_frame_t *f, uint32_t timeout_ms);

/** @brief Wait up to @p timeout_ms for a frame. False if none arrived. */
bool can_bus_recv(can_frame_t *f, uint32_t timeout_ms);

/** @brief Call regularly: recovers from bus-off without a dedicated task,
 *         and drains driver alerts (RX_QUEUE_FULL, bus-off, error-passive). */
void can_bus_poll_recovery(void);

/** @brief Log the controller's state and error counters (for debugging the bus). */
void can_bus_log_status(void);

/** @brief Frames lost to RX_QUEUE_FULL since can_bus_init(). For telemetry
 *         (Master status frame) rather than just log lines. */
uint32_t can_bus_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_CAN_BUS_H */