/*
 * NMEA 2000 framing helpers.
 *
 * This module contains no ESP32-specific code so that it can be compiled and
 * tested on a normal PC.  It handles 29-bit CAN identifiers, fast-packet
 * reassembly and the small number of PGNs used by the AC42 bridge.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define N2K_MAX_PAYLOAD       223u
#define N2K_FASTPACKET_SLOTS  8u

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} n2k_frame_t;

typedef struct {
    bool     active;
    uint32_t pgn;
    uint8_t  src;
    uint8_t  seq;
    uint8_t  next_frame;
    uint16_t total_len;
    uint16_t len;
    uint32_t last_ms;
    uint8_t  data[N2K_MAX_PAYLOAD];
} n2k_fp_slot_t;

typedef struct {
    n2k_fp_slot_t slots[N2K_FASTPACKET_SLOTS];
} n2k_fp_pool_t;

typedef enum {
    N2K_FP_NOT_FAST = 0,
    N2K_FP_INCOMPLETE,
    N2K_FP_COMPLETE,
    N2K_FP_ERROR
} n2k_fp_result_t;

static inline uint32_t n2k_make_id(uint8_t priority, uint32_t pgn, uint8_t src)
{
    return ((uint32_t)(priority & 0x7u) << 26) |
           ((pgn & 0x3FFFFu) << 8) |
           (uint32_t)src;
}

static inline uint32_t n2k_id_pgn(uint32_t id)
{
    return (id >> 8) & 0x3FFFFu;
}

static inline uint8_t n2k_id_src(uint32_t id)
{
    return (uint8_t)(id & 0xFFu);
}

static inline uint8_t n2k_id_priority(uint32_t id)
{
    return (uint8_t)((id >> 26) & 0x7u);
}

/* PGNs handled by the AC42 bridge that use NMEA 2000 fast-packet. */
bool n2k_is_fast_packet(uint32_t pgn);

void n2k_fp_pool_init(n2k_fp_pool_t *pool);

/*
 * Feed one CAN frame.  For fast-packet PGNs the result is COMPLETE when the
 * reassembled payload is available in out/out_len.  The caller must pass a
 * buffer of at least N2K_MAX_PAYLOAD bytes.
 */
n2k_fp_result_t n2k_fp_feed(n2k_fp_pool_t *pool,
                            uint32_t id,
                            const uint8_t *data,
                            uint8_t dlc,
                            uint32_t now_ms,
                            uint8_t *out,
                            size_t out_cap,
                            size_t *out_len);

void n2k_fp_expire(n2k_fp_pool_t *pool, uint32_t now_ms, uint32_t max_age_ms);

#ifdef __cplusplus
}
#endif
