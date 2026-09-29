/**
 * @file ttc.h
 * @brief Master's TT&C link handling: bare minimum to prove Master and a
 *        ground script can talk. Deliberately simple: no queues, no CAN
 *        forwarding, no flight-manager gating yet:
 *          - ttc_pack_downlink() is a pure helper comms_task uses to mirror
 *            the CAN heartbeat it already sends, over UDP
 *          - ttc_uplink_task() is a TCP listener that logs and ACKs
 *            whatever ground sends. It doesn't forward anything to CAN... :3
 *
 * Lives here (not libs/) because it owns a task and sockets, and isn't just a helper function
 */
#ifndef HELIA_MASTER_TTC_H
#define HELIA_MASTER_TTC_H

#include <stddef.h>
#include <stdint.h>
#include "can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Exact size of a packed downlink packet (4-byte header + 11-byte
 *         frame).
 * Size the buffer passed to ttc_pack_downlink() with this */
#define TTC_DL_PACKET_LEN 15u

/**
 * @brief Pack one CAN frame into the SED UDP downlink wire format
 *        (header + 11-byte frame). Pure byte-packing, no I/O
 * @return Bytes written to @p pkt (TTC_DL_PACKET_LEN), or 0 if @p pkt_len
 *         is too small.
 */
size_t ttc_pack_downlink(uint8_t *pkt, size_t pkt_len, uint8_t seq, const can_frame_t *f);

/**
 * @brief TCP uplink listener. Accepts one ground connection at a time,
 *        logs and ACKs each command it parses. Pinned to core 0 by
 *        setup.c, alongside comms_task
 */
void ttc_uplink_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_MASTER_TTC_H */