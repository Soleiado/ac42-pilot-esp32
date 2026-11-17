/*
 * Simrad AC42 / Simnet protocol engine.
 *
 * The code in this file is intentionally independent from Arduino and from
 * the ESP32.  It can be unit-tested on a PC and is shared by the firmware.
 *
 * Protocol sources:
 *   - signalk-ac42-autopilot (Apache-2.0), protocol reverse-engineered on a
 *     real AC42;
 *   - canboat PGN definitions (Apache-2.0).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "n2k_codec.h"

#define AC42_MAX_CONTROLLERS 4u

#define AC42_PGN_AP_COMMAND      130850u
#define AC42_PGN_AP_REPLY        130851u
#define AC42_PGN_KEY_VALUE       130845u
#define AC42_PGN_DEVICE_STATUS   65305u
#define AC42_PGN_PILOT_MODE      65305u
#define AC42_PGN_MODE_STATE      65340u
#define AC42_PGN_AP_ANGLE        65341u
#define AC42_PGN_HEADING_TRACK   127237u
#define AC42_PGN_VESSEL_HEADING  127250u
#define AC42_PGN_RUDDER          127245u
#define AC42_PGN_SPEED           128259u
#define AC42_PGN_WATER_DEPTH     128267u
#define AC42_PGN_COG_SOG         129026u
#define AC42_PGN_WIND            130306u

#define AC42_KEY_RESPONSE_LOW   0x1119u
#define AC42_KEY_RESPONSE_HIGH  0x111Au
#define AC42_KEY_RESPONSE_WIND  0x111Cu

#define AC42_LEVEL_LOW   0u
#define AC42_LEVEL_HIGH  1u
#define AC42_LEVEL_WIND  2u
#define AC42_LEVEL_COUNT 3u

typedef enum {
    AC42_MODE_UNKNOWN = 0,
    AC42_MODE_STANDBY,
    AC42_MODE_AUTO,
    AC42_MODE_WIND,
    AC42_MODE_NAV,
    AC42_MODE_NO_DRIFT,
    AC42_MODE_NFU
} ac42_mode_t;

typedef enum {
    AC42_RESPONSE_AUTO = 0,
    AC42_RESPONSE_LOW,
    AC42_RESPONSE_HIGH
} ac42_response_set_t;

typedef enum {
    AC42_SIDE_PORT = 0, /* lofer / - */
    AC42_SIDE_STBD = 1  /* abattre / + */
} ac42_side_t;

typedef enum {
    AC42_RUDDER_PORT = 0,
    AC42_RUDDER_STBD,
    AC42_RUDDER_STOP
} ac42_rudder_dir_t;

typedef struct {
    bool     valid;
    uint32_t ms;
    float    value;
} ac42_value_t;

typedef struct {
    /* NMEA 2000 addresses. */
    uint8_t  ap_addr;
    bool     ap_valid;
    uint32_t ap_ms;
    uint8_t  ap_model;
    uint8_t  controller_addr[AC42_MAX_CONTROLLERS];
    uint32_t controller_ms[AC42_MAX_CONTROLLERS];

    /* Pilot state. */
    ac42_mode_t mode;
    bool        mode_valid;
    uint32_t    mode_ms;
    bool        engaged;
    bool        engaged_valid;
    uint32_t    engaged_ms;
    uint16_t    mode_bits;
    bool        mode_bits_valid;
    uint32_t    mode_bits_ms;
    bool        response_high;
    bool        response_auto;
    bool        response_valid;
    uint32_t    response_ms;

    /* PGN 130845 settings read from the AC42 / written by an MFD. */
    uint8_t  levels[AC42_LEVEL_COUNT];
    bool     level_valid[AC42_LEVEL_COUNT];
    uint32_t levels_ms;

    /* Pilot setpoints. */
    ac42_value_t target_mag_deg;
    ac42_value_t target_true_deg;
    ac42_value_t wind_target_deg;
    uint8_t      target_src;

    /* Instruments. */
    ac42_value_t heading_mag_deg;
    ac42_value_t heading_true_deg;
    ac42_value_t variation_deg;
    ac42_value_t awa_deg;
    ac42_value_t aws_kn;
    ac42_value_t twa_deg;
    ac42_value_t tws_kn;
    ac42_value_t twd_mag_deg;
    ac42_value_t twd_true_deg;
    ac42_value_t sog_kn;
    ac42_value_t stw_kn;
    ac42_value_t cog_mag_deg;
    ac42_value_t cog_true_deg;
    ac42_value_t depth_m;
    ac42_value_t rudder_deg;
    ac42_value_t commanded_rudder_deg;

    /* Fast-packet reassembly and counters. */
    n2k_fp_pool_t fp;
    uint32_t      rx_frames;
    uint32_t      last_rx_ms;
    uint32_t      unsupported_frames;
} ac42_state_t;

void ac42_state_init(ac42_state_t *st);
void ac42_state_on_frame(ac42_state_t *st, const n2k_frame_t *frame, uint32_t now_ms);
void ac42_state_tick(ac42_state_t *st, uint32_t now_ms);

const char *ac42_mode_name(ac42_mode_t mode);
const char *ac42_response_set_name(ac42_response_set_t set);
bool ac42_mode_is_engaged(ac42_mode_t mode);

ac42_mode_t ac42_effective_mode(const ac42_state_t *st, uint32_t now_ms,
                                uint32_t stale_ms);
bool ac42_effective_engaged(const ac42_state_t *st, uint32_t now_ms,
                            uint32_t stale_ms);

float ac42_wrap180(float deg);
float ac42_wrap360(float deg);
bool ac42_value_fresh(const ac42_value_t *v, uint32_t now_ms, uint32_t max_age_ms);
bool ac42_levels_stale(const ac42_state_t *st, uint32_t now_ms, uint32_t max_age_ms);

/*
 * Returns the most recently seen controller heartbeat source, excluding the
 * autopilot itself.  Returns false when none is fresh.
 */
bool ac42_best_controller(const ac42_state_t *st,
                          uint32_t now_ms,
                          uint32_t stale_ms,
                          uint8_t *out_addr);

/* ----- Frame builders ------------------------------------------------- */

/*
 * All builders take a 3-bit sequence group (0..7) chosen by the caller.  The
 * caller is responsible for incrementing it between messages.
 */
size_t ac42_build_mode(uint8_t sa, uint8_t target, ac42_mode_t mode,
                       uint8_t seq, n2k_frame_t out[2]);

size_t ac42_build_course(uint8_t sa, uint8_t target, ac42_side_t side,
                         int degrees, bool wind_mode,
                         uint8_t seq, n2k_frame_t out[2]);

size_t ac42_build_tack(uint8_t sa, uint8_t target, ac42_side_t side,
                       uint8_t seq, n2k_frame_t out[2]);

size_t ac42_build_response(uint8_t sa, uint8_t target,
                           ac42_response_set_t set,
                           uint8_t seq, n2k_frame_t out[2]);

size_t ac42_build_rudder(uint8_t sa, uint8_t target,
                         ac42_rudder_dir_t dir,
                         uint8_t seq, n2k_frame_t out[2]);

size_t ac42_build_key_query(uint8_t sa, uint8_t target, uint16_t key,
                            uint8_t seq, n2k_frame_t out[3]);

#ifdef __cplusplus
}
#endif
