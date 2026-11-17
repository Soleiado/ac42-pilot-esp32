#include "ac42_protocol.h"

#include <math.h>
#include <string.h>

#define AC42_MANUFACTURER 1857u
#define AC42_INDUSTRY     4u
#define AC42_MPS_TO_KN    1.9438444924406046f
#define AC42_DEG_PER_RAD  57.29577951308232f
#define AC42_RAD_PER_DEG  0.017453292519943295f

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t get_i16(const uint8_t *p)
{
    return (int16_t)get_u16(p);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool is_u16_unavailable(uint16_t v)
{
    return v == 0xFFFFu;
}

static bool is_i16_unavailable(int16_t v)
{
    const uint16_t u = (uint16_t)v;
    return u == 0x7FFFu || u == 0xFFFFu;
}

static bool simnet_header(const uint8_t *p, size_t len)
{
    if (len < 2) {
        return false;
    }
    const uint16_t h = get_u16(p);
    return (h & 0x7FFu) == AC42_MANUFACTURER &&
           ((h >> 13) & 0x7u) == AC42_INDUSTRY;
}

static void set_value(ac42_value_t *v, float value, uint32_t now_ms)
{
    v->valid = true;
    v->ms = now_ms;
    v->value = value;
}

float ac42_wrap360(float deg)
{
    deg = fmodf(deg, 360.0f);
    if (deg < 0.0f) {
        deg += 360.0f;
    }
    return deg;
}

float ac42_wrap180(float deg)
{
    deg = ac42_wrap360(deg);
    if (deg > 180.0f) {
        deg -= 360.0f;
    }
    return deg;
}

bool ac42_value_fresh(const ac42_value_t *v, uint32_t now_ms, uint32_t max_age_ms)
{
    return v != NULL && v->valid && (uint32_t)(now_ms - v->ms) <= max_age_ms;
}

bool ac42_mode_is_engaged(ac42_mode_t mode)
{
    return mode == AC42_MODE_AUTO || mode == AC42_MODE_WIND ||
           mode == AC42_MODE_NAV || mode == AC42_MODE_NO_DRIFT;
}

const char *ac42_mode_name(ac42_mode_t mode)
{
    switch (mode) {
    case AC42_MODE_STANDBY:  return "Standby";
    case AC42_MODE_AUTO:     return "Auto";
    case AC42_MODE_WIND:     return "Wind";
    case AC42_MODE_NAV:      return "Nav";
    case AC42_MODE_NO_DRIFT: return "No Drift";
    case AC42_MODE_NFU:      return "NFU";
    default:                 return "Unknown";
    }
}

const char *ac42_response_set_name(ac42_response_set_t set)
{
    switch (set) {
    case AC42_RESPONSE_AUTO: return "auto";
    case AC42_RESPONSE_LOW:  return "low";
    case AC42_RESPONSE_HIGH: return "high";
    default:                 return "auto";
    }
}

ac42_mode_t ac42_effective_mode(const ac42_state_t *st, uint32_t now_ms,
                                uint32_t stale_ms)
{
    if (st == NULL || !st->mode_valid ||
        (uint32_t)(now_ms - st->mode_ms) > stale_ms) {
        return AC42_MODE_UNKNOWN;
    }
    return st->mode;
}

bool ac42_effective_engaged(const ac42_state_t *st, uint32_t now_ms,
                            uint32_t stale_ms)
{
    if (st == NULL) {
        return false;
    }
    if (st->engaged_valid &&
        (uint32_t)(now_ms - st->engaged_ms) <= stale_ms) {
        return st->engaged;
    }
    const ac42_mode_t mode = ac42_effective_mode(st, now_ms, stale_ms);
    return ac42_mode_is_engaged(mode);
}

/* ------------------------------------------------------------------------- */
/* Address tracking                                                          */
/* ------------------------------------------------------------------------- */

static void controller_remove(ac42_state_t *st, uint8_t addr)
{
    for (size_t i = 0; i < AC42_MAX_CONTROLLERS; i++) {
        if (st->controller_ms[i] != 0 && st->controller_addr[i] == addr) {
            st->controller_ms[i] = 0;
        }
    }
}

static void controller_touch(ac42_state_t *st, uint8_t addr, uint32_t now_ms)
{
    if (st->ap_valid && addr == st->ap_addr) {
        return;
    }

    size_t free_slot = AC42_MAX_CONTROLLERS;
    size_t oldest_slot = 0;
    for (size_t i = 0; i < AC42_MAX_CONTROLLERS; i++) {
        if (st->controller_ms[i] != 0 && st->controller_addr[i] == addr) {
            st->controller_ms[i] = now_ms;
            return;
        }
        if (st->controller_ms[i] == 0 && free_slot == AC42_MAX_CONTROLLERS) {
            free_slot = i;
        }
        if (st->controller_ms[i] != 0 &&
            (int32_t)(st->controller_ms[i] - st->controller_ms[oldest_slot]) < 0) {
            oldest_slot = i;
        }
    }

    const size_t slot = (free_slot < AC42_MAX_CONTROLLERS) ? free_slot : oldest_slot;
    st->controller_addr[slot] = addr;
    st->controller_ms[slot] = now_ms;
}

static void set_ap(ac42_state_t *st, uint8_t addr, uint8_t model,
                   uint32_t now_ms)
{
    if (!st->ap_valid || st->ap_addr != addr) {
        st->target_mag_deg.valid = false;
        st->target_true_deg.valid = false;
        st->wind_target_deg.valid = false;
    }
    st->ap_addr = addr;
    st->ap_valid = true;
    st->ap_ms = now_ms;
    if (model != 0xFF) {
        st->ap_model = model;
    }
    controller_remove(st, addr);
}

bool ac42_best_controller(const ac42_state_t *st, uint32_t now_ms,
                          uint32_t stale_ms, uint8_t *out_addr)
{
    if (st == NULL) {
        return false;
    }

    bool found = false;
    uint32_t best_age = UINT32_MAX;
    uint8_t best_addr = 0;

    for (size_t i = 0; i < AC42_MAX_CONTROLLERS; i++) {
        if (st->controller_ms[i] == 0) {
            continue;
        }
        if (st->ap_valid && st->controller_addr[i] == st->ap_addr) {
            continue;
        }
        const uint32_t age = (uint32_t)(now_ms - st->controller_ms[i]);
        if (age <= stale_ms && age < best_age) {
            best_age = age;
            best_addr = st->controller_addr[i];
            found = true;
        }
    }

    if (found && out_addr != NULL) {
        *out_addr = best_addr;
    }
    return found;
}

/* ------------------------------------------------------------------------- */
/* State decoding                                                            */
/* ------------------------------------------------------------------------- */

static ac42_mode_t mode_from_pilot_bits(uint16_t bits)
{
    /* canboat SIMNET_AP_MODE_BITFIELD: bits 3, 4, 6, 8 and 10. */
    if (bits & (1u << 10)) return AC42_MODE_WIND;
    if (bits & (1u << 4))  return AC42_MODE_AUTO;
    if (bits & (1u << 6))  return AC42_MODE_NAV;
    if (bits & (1u << 8))  return AC42_MODE_NO_DRIFT;
    if (bits & (1u << 3))  return AC42_MODE_STANDBY;
    return AC42_MODE_UNKNOWN;
}

static ac42_mode_t mode_from_mode_state(uint8_t mode)
{
    switch (mode) {
    case 0: return AC42_MODE_STANDBY;
    case 1: return AC42_MODE_AUTO;
    case 4: return AC42_MODE_WIND;
    case 5: return AC42_MODE_NFU;
    case 6: return AC42_MODE_NAV;
    default: return AC42_MODE_UNKNOWN;
    }
}

static void update_heading(ac42_state_t *st, float heading_deg, uint8_t ref,
                           float variation_deg, bool variation_valid,
                           uint32_t now_ms)
{
    if (ref == 1) { /* magnetic */
        set_value(&st->heading_mag_deg, ac42_wrap360(heading_deg), now_ms);
        if (variation_valid) {
            set_value(&st->heading_true_deg,
                      ac42_wrap360(heading_deg + variation_deg), now_ms);
        }
    } else if (ref == 0) { /* true */
        set_value(&st->heading_true_deg, ac42_wrap360(heading_deg), now_ms);
        if (variation_valid) {
            set_value(&st->heading_mag_deg,
                      ac42_wrap360(heading_deg - variation_deg), now_ms);
        }
    }
}

static void update_cog(ac42_state_t *st, float cog_deg, uint8_t ref,
                       float variation_deg, bool variation_valid,
                       uint32_t now_ms)
{
    if (ref == 1) {
        set_value(&st->cog_mag_deg, ac42_wrap360(cog_deg), now_ms);
        if (variation_valid) {
            set_value(&st->cog_true_deg,
                      ac42_wrap360(cog_deg + variation_deg), now_ms);
        }
    } else if (ref == 0) {
        set_value(&st->cog_true_deg, ac42_wrap360(cog_deg), now_ms);
        if (variation_valid) {
            set_value(&st->cog_mag_deg,
                      ac42_wrap360(cog_deg - variation_deg), now_ms);
        }
    }
}

static void update_target(ac42_state_t *st, float target_deg, uint8_t ref,
                          float variation_deg, bool variation_valid,
                          uint32_t now_ms, uint8_t src)
{
    st->target_src = src;
    if (ref == 1) {
        set_value(&st->target_mag_deg, ac42_wrap360(target_deg), now_ms);
        if (variation_valid) {
            set_value(&st->target_true_deg,
                      ac42_wrap360(target_deg + variation_deg), now_ms);
        }
    } else if (ref == 0) {
        set_value(&st->target_true_deg, ac42_wrap360(target_deg), now_ms);
        if (variation_valid) {
            set_value(&st->target_mag_deg,
                      ac42_wrap360(target_deg - variation_deg), now_ms);
        }
    }
}

static void process_65305(ac42_state_t *st, uint8_t src,
                          const uint8_t *p, size_t len, uint32_t now_ms)
{
    if (len < 4 || !simnet_header(p, len)) {
        return;
    }
    const uint8_t model = p[2];
    const uint8_t report = p[3];

    if (report == 3 || report == 11) { /* controller heartbeat */
        controller_touch(st, src, now_ms);
        return;
    }
    if (report == 2) { /* Device Status */
        if (len < 5) {
            return;
        }
        set_ap(st, src, model, now_ms);
        const uint8_t status = p[4];
        st->engaged = (status == 16);
        st->engaged_valid = true;
        st->engaged_ms = now_ms;
        return;
    }
    if (report == 10) { /* Pilot Mode */
        if (len < 6) {
            return;
        }
        set_ap(st, src, model, now_ms);
        const uint16_t bits = get_u16(p + 4);
        st->mode_bits = bits;
        st->mode_bits_valid = true;
        st->mode_bits_ms = now_ms;
        st->response_high = (bits & (1u << 1)) != 0;
        st->response_auto = (bits & (1u << 2)) != 0;
        st->response_valid = true;
        st->response_ms = now_ms;

        const ac42_mode_t mode = mode_from_pilot_bits(bits);
        if (mode != AC42_MODE_UNKNOWN) {
            st->mode = mode;
            st->mode_valid = true;
            st->mode_ms = now_ms;
            if (mode == AC42_MODE_STANDBY) {
                st->engaged = false;
                st->engaged_valid = true;
                st->engaged_ms = now_ms;
            }
        }
    }
}

static void process_65340(ac42_state_t *st, uint8_t src,
                          const uint8_t *p, size_t len, uint32_t now_ms)
{
    if (len < 4 || !simnet_header(p, len)) {
        return;
    }
    set_ap(st, src, 0xFF, now_ms);
    const uint8_t mode_class = p[2];
    const uint8_t raw_mode = p[3];

    st->engaged = (mode_class == 16);
    st->engaged_valid = true;
    st->engaged_ms = now_ms;

    ac42_mode_t mode = mode_from_mode_state(raw_mode);
    if (mode_class == 0) {
        mode = AC42_MODE_STANDBY;
    }
    if (mode != AC42_MODE_UNKNOWN) {
        st->mode = mode;
        st->mode_valid = true;
        st->mode_ms = now_ms;
    }
}

static void process_65341(ac42_state_t *st, uint8_t src,
                          const uint8_t *p, size_t len, uint32_t now_ms)
{
    if (len < 8 || !simnet_header(p, len)) {
        return;
    }
    set_ap(st, src, 0xFF, now_ms);

    const uint8_t mode = p[4];
    const uint16_t angle_raw = get_u16(p + 6);
    if (is_u16_unavailable(angle_raw)) {
        return;
    }
    const float angle_deg = (float)angle_raw * 0.0001f * AC42_DEG_PER_RAD;

    /* 3 = Wind in SIMNET_AP_MODE.  The angle is the commanded apparent wind
     * angle, normalised to port/starboard (-180..180). */
    if (mode == 3) {
        set_value(&st->wind_target_deg, ac42_wrap180(angle_deg), now_ms);
    }
}

static void process_127237(ac42_state_t *st, uint8_t src,
                           const uint8_t *p, size_t len, uint32_t now_ms)
{
    if (len < 7) {
        return;
    }
    /* The AC42 only broadcasts this while it is the autopilot; ignore other
     * sources once the AC42 address is known. */
    if (st->ap_valid && src != st->ap_addr) {
        return;
    }

    const uint8_t ref = (uint8_t)((p[1] >> 6) & 0x03u);
    const uint16_t target_raw = get_u16(p + 5);
    const bool variation_valid = ac42_value_fresh(&st->variation_deg, now_ms, 10000);
    const float variation = st->variation_deg.value;

    if (!is_u16_unavailable(target_raw)) {
        const float target_deg = (float)target_raw * 0.0001f * AC42_DEG_PER_RAD;
        update_target(st, target_deg, ref, variation, variation_valid,
                      now_ms, src);
    }

    if (len >= 5) {
        const int16_t rudder_raw = get_i16(p + 3);
        if (!is_i16_unavailable(rudder_raw)) {
            set_value(&st->commanded_rudder_deg,
                      (float)rudder_raw * 0.0001f * AC42_DEG_PER_RAD, now_ms);
        }
    }
}

static void process_127250(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 7) {
        return;
    }

    const uint16_t variation_raw = get_u16(p + 5);
    bool variation_valid = false;
    if (!is_i16_unavailable((int16_t)variation_raw)) {
        st->variation_deg.value =
            (float)(int16_t)variation_raw * 0.0001f * AC42_DEG_PER_RAD;
        st->variation_deg.valid = true;
        st->variation_deg.ms = now_ms;
        variation_valid = true;
    }

    const uint16_t heading_raw = get_u16(p + 1);
    if (is_u16_unavailable(heading_raw)) {
        return;
    }
    const float heading_deg = (float)heading_raw * 0.0001f * AC42_DEG_PER_RAD;
    const uint8_t ref = (len >= 8) ? (uint8_t)(p[7] & 0x03u) : 1u;
    update_heading(st, heading_deg, ref, st->variation_deg.value,
                   variation_valid, now_ms);
}

static void process_129026(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 6) {
        return;
    }

    const bool variation_valid = ac42_value_fresh(&st->variation_deg, now_ms, 10000);
    const float variation = st->variation_deg.value;
    const uint16_t cog_raw = get_u16(p + 2);
    if (!is_u16_unavailable(cog_raw)) {
        const float cog_deg = (float)cog_raw * 0.0001f * AC42_DEG_PER_RAD;
        const uint8_t ref = (uint8_t)(p[1] & 0x03u);
        update_cog(st, cog_deg, ref, variation, variation_valid, now_ms);
    }

    const uint16_t sog_raw = get_u16(p + 4);
    if (!is_u16_unavailable(sog_raw)) {
        set_value(&st->sog_kn, (float)sog_raw * 0.01f * AC42_MPS_TO_KN, now_ms);
    }
}

static void process_128259(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 5) {
        return;
    }
    const uint16_t stw_raw = get_u16(p + 1);
    if (!is_u16_unavailable(stw_raw)) {
        set_value(&st->stw_kn, (float)stw_raw * 0.01f * AC42_MPS_TO_KN, now_ms);
    }
    const uint16_t sog_raw = get_u16(p + 3);
    if (!is_u16_unavailable(sog_raw) &&
        !ac42_value_fresh(&st->sog_kn, now_ms, 2000)) {
        set_value(&st->sog_kn, (float)sog_raw * 0.01f * AC42_MPS_TO_KN, now_ms);
    }
}

static void process_130306(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 6) {
        return;
    }

    const uint16_t speed_raw = get_u16(p + 1);
    const uint16_t angle_raw = get_u16(p + 3);
    const uint8_t ref = (uint8_t)(p[5] & 0x07u);
    if (is_u16_unavailable(angle_raw)) {
        return;
    }
    const float angle_deg = (float)angle_raw * 0.0001f * AC42_DEG_PER_RAD;
    float speed_kn = 0.0f;
    bool speed_valid = false;
    if (!is_u16_unavailable(speed_raw)) {
        speed_kn = (float)speed_raw * 0.01f * AC42_MPS_TO_KN;
        speed_valid = true;
    }

    if (ref == 2) { /* apparent */
        set_value(&st->awa_deg, ac42_wrap180(angle_deg), now_ms);
        if (speed_valid) {
            set_value(&st->aws_kn, speed_kn, now_ms);
        }
    } else if (ref == 3 || ref == 4) { /* true, boat/water referenced */
        set_value(&st->twa_deg, ac42_wrap180(angle_deg), now_ms);
        if (speed_valid) {
            set_value(&st->tws_kn, speed_kn, now_ms);
        }
    } else if (ref == 0 || ref == 1) { /* true/magnetic wind direction */
        const bool variation_valid =
            ac42_value_fresh(&st->variation_deg, now_ms, 10000);
        const float variation = st->variation_deg.value;
        float twd_true = 0.0f;
        float twd_mag = 0.0f;
        bool have_true = false;
        bool have_mag = false;

        if (ref == 0) {
            twd_true = ac42_wrap360(angle_deg);
            set_value(&st->twd_true_deg, twd_true, now_ms);
            have_true = true;
            if (variation_valid) {
                twd_mag = ac42_wrap360(twd_true - variation);
                set_value(&st->twd_mag_deg, twd_mag, now_ms);
                have_mag = true;
            }
        } else {
            twd_mag = ac42_wrap360(angle_deg);
            set_value(&st->twd_mag_deg, twd_mag, now_ms);
            have_mag = true;
            if (variation_valid) {
                twd_true = ac42_wrap360(twd_mag + variation);
                set_value(&st->twd_true_deg, twd_true, now_ms);
                have_true = true;
            }
        }

        if (ac42_value_fresh(&st->heading_true_deg, now_ms, 5000) && have_true) {
            set_value(&st->twa_deg,
                      ac42_wrap180(twd_true - st->heading_true_deg.value), now_ms);
            if (speed_valid) {
                set_value(&st->tws_kn, speed_kn, now_ms);
            }
        } else if (ac42_value_fresh(&st->heading_mag_deg, now_ms, 5000) && have_mag) {
            set_value(&st->twa_deg,
                      ac42_wrap180(twd_mag - st->heading_mag_deg.value), now_ms);
            if (speed_valid) {
                set_value(&st->tws_kn, speed_kn, now_ms);
            }
        }
    }
}

static void process_128267(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 5) {
        return;
    }
    const uint32_t depth_raw = get_u32(p + 1);
    if (depth_raw != 0xFFFFFFFFu) {
        set_value(&st->depth_m, (float)depth_raw * 0.01f, now_ms);
    }
}

static void process_127245(ac42_state_t *st, const uint8_t *p, size_t len,
                           uint32_t now_ms)
{
    if (len < 6) {
        return;
    }
    const int16_t position_raw = get_i16(p + 4);
    if (!is_i16_unavailable(position_raw)) {
        set_value(&st->rudder_deg,
                  (float)position_raw * 0.0001f * AC42_DEG_PER_RAD, now_ms);
    }
    const int16_t order_raw = get_i16(p + 2);
    if (!is_i16_unavailable(order_raw)) {
        set_value(&st->commanded_rudder_deg,
                  (float)order_raw * 0.0001f * AC42_DEG_PER_RAD, now_ms);
    }
}

static void process_130845(ac42_state_t *st, uint8_t src,
                           const uint8_t *p, size_t len, uint32_t now_ms)
{
    if (len < 11 || !simnet_header(p, len)) {
        return;
    }
    const uint8_t target = p[2];
    if (st->ap_valid && target != st->ap_addr) {
        return;
    }

    const uint32_t key = (uint32_t)p[6] | ((uint32_t)p[7] << 8) |
                         ((uint32_t)p[8] << 16);
    const uint8_t op = p[9];
    const uint8_t value = p[10];
    if ((op != 1 && op != 2) || value < 1 || value > 10) {
        return;
    }

    if (op == 2 && !st->ap_valid) {
        set_ap(st, src, 0xFF, now_ms);
    }

    int level = -1;
    if (key == AC42_KEY_RESPONSE_LOW) {
        level = AC42_LEVEL_LOW;
    } else if (key == AC42_KEY_RESPONSE_HIGH) {
        level = AC42_LEVEL_HIGH;
    } else if (key == AC42_KEY_RESPONSE_WIND) {
        level = AC42_LEVEL_WIND;
    }
    if (level >= 0) {
        st->levels[level] = value;
        st->level_valid[level] = true;
        st->levels_ms = now_ms;
    }
}

/* ------------------------------------------------------------------------- */
/* Public state API                                                          */
/* ------------------------------------------------------------------------- */

void ac42_state_init(ac42_state_t *st)
{
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
    n2k_fp_pool_init(&st->fp);
}

void ac42_state_tick(ac42_state_t *st, uint32_t now_ms)
{
    if (st == NULL) {
        return;
    }
    n2k_fp_expire(&st->fp, now_ms, 1500);
    for (size_t i = 0; i < AC42_MAX_CONTROLLERS; i++) {
        if (st->controller_ms[i] != 0 &&
            (uint32_t)(now_ms - st->controller_ms[i]) > 60000u) {
            st->controller_ms[i] = 0;
        }
    }
}

bool ac42_levels_stale(const ac42_state_t *st, uint32_t now_ms,
                       uint32_t max_age_ms)
{
    if (st == NULL) {
        return true;
    }
    for (size_t i = 0; i < AC42_LEVEL_COUNT; i++) {
        if (!st->level_valid[i]) {
            return true;
        }
    }
    return (uint32_t)(now_ms - st->levels_ms) > max_age_ms;
}

void ac42_state_on_frame(ac42_state_t *st, const n2k_frame_t *frame,
                         uint32_t now_ms)
{
    if (st == NULL || frame == NULL || frame->dlc < 2) {
        return;
    }

    const uint32_t pgn = n2k_id_pgn(frame->id);
    const uint8_t src = n2k_id_src(frame->id);

    if (pgn != AC42_PGN_DEVICE_STATUS && pgn != AC42_PGN_MODE_STATE &&
        pgn != AC42_PGN_AP_ANGLE && pgn != AC42_PGN_HEADING_TRACK &&
        pgn != AC42_PGN_VESSEL_HEADING && pgn != AC42_PGN_RUDDER &&
        pgn != AC42_PGN_SPEED && pgn != AC42_PGN_WATER_DEPTH &&
        pgn != AC42_PGN_COG_SOG && pgn != AC42_PGN_WIND &&
        pgn != AC42_PGN_KEY_VALUE) {
        st->unsupported_frames++;
        return;
    }

    st->rx_frames++;
    st->last_rx_ms = now_ms;

    uint8_t payload[N2K_MAX_PAYLOAD];
    const uint8_t *p = frame->data;
    size_t len = frame->dlc;

    if (n2k_is_fast_packet(pgn)) {
        size_t assembled = 0;
        const n2k_fp_result_t result =
            n2k_fp_feed(&st->fp, frame->id, frame->data, frame->dlc,
                        now_ms, payload, sizeof(payload), &assembled);
        if (result != N2K_FP_COMPLETE) {
            return;
        }
        p = payload;
        len = assembled;
    }

    switch (pgn) {
    case AC42_PGN_DEVICE_STATUS:
        process_65305(st, src, p, len, now_ms);
        break;
    case AC42_PGN_MODE_STATE:
        process_65340(st, src, p, len, now_ms);
        break;
    case AC42_PGN_AP_ANGLE:
        process_65341(st, src, p, len, now_ms);
        break;
    case AC42_PGN_HEADING_TRACK:
        process_127237(st, src, p, len, now_ms);
        break;
    case AC42_PGN_VESSEL_HEADING:
        process_127250(st, p, len, now_ms);
        break;
    case AC42_PGN_RUDDER:
        process_127245(st, p, len, now_ms);
        break;
    case AC42_PGN_SPEED:
        process_128259(st, p, len, now_ms);
        break;
    case AC42_PGN_WATER_DEPTH:
        process_128267(st, p, len, now_ms);
        break;
    case AC42_PGN_COG_SOG:
        process_129026(st, p, len, now_ms);
        break;
    case AC42_PGN_WIND:
        process_130306(st, p, len, now_ms);
        break;
    case AC42_PGN_KEY_VALUE:
        process_130845(st, src, p, len, now_ms);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Frame builders                                                            */
/* ------------------------------------------------------------------------- */

static size_t build_ap_command(uint8_t sa, uint8_t target, uint8_t cmd_class,
                               const uint8_t payload6[6], uint8_t seq,
                               n2k_frame_t out[2])
{
    const uint8_t s = (uint8_t)((seq & 0x07u) << 5);
    out[0].id = n2k_make_id(2, AC42_PGN_AP_COMMAND, sa);
    out[0].dlc = 8;
    out[0].data[0] = (uint8_t)(s | 0u);
    out[0].data[1] = 0x0C; /* 12-byte fast packet */
    out[0].data[2] = 0x41;
    out[0].data[3] = 0x9F;
    out[0].data[4] = target;
    out[0].data[5] = 0xFF;
    out[0].data[6] = 0xFF;
    out[0].data[7] = cmd_class;

    out[1].id = out[0].id;
    out[1].dlc = 8;
    out[1].data[0] = (uint8_t)(s | 1u);
    memcpy(&out[1].data[1], payload6, 6);
    out[1].data[7] = 0xFF;
    return 2;
}

size_t ac42_build_mode(uint8_t sa, uint8_t target, ac42_mode_t mode,
                       uint8_t seq, n2k_frame_t out[2])
{
    uint8_t payload[6] = {0};
    switch (mode) {
    case AC42_MODE_AUTO:
        payload[0] = 0x09; /* Heading mode */
        payload[1] = 0x00;
        payload[2] = 0xFF;
        payload[3] = 0xFF;
        payload[4] = 0xFF;
        payload[5] = 0xFF;
        break;
    case AC42_MODE_WIND:
        payload[0] = 0x0F; /* Wind mode */
        break;
    case AC42_MODE_STANDBY:
        payload[0] = 0x06; /* Standby */
        break;
    default:
        return 0;
    }
    return build_ap_command(sa, target, 0x0A, payload, seq, out);
}

size_t ac42_build_course(uint8_t sa, uint8_t target, ac42_side_t side,
                         int degrees, bool wind_mode,
                         uint8_t seq, n2k_frame_t out[2])
{
    uint16_t angle_raw;
    if (degrees == 1) {
        angle_raw = 0x00AE; /* validated on a real B&G controller */
    } else if (degrees == 10) {
        angle_raw = 0x06D1;
    } else {
        return 0;
    }

    /* Direction bytes are mode-dependent.  This is counter-intuitive but was
     * validated against a real AC42:
     *   Auto: STBD/+ => 0x03, PORT/- => 0x02
     *   Wind: STBD/+ => 0x02, PORT/- => 0x03
     */
    uint8_t direction;
    if (side == AC42_SIDE_STBD) {
        direction = wind_mode ? 0x02 : 0x03;
    } else {
        direction = wind_mode ? 0x03 : 0x02;
    }

    const uint8_t payload[6] = {
        0x1A, /* Change course */
        0x00,
        direction,
        (uint8_t)(angle_raw & 0xFF),
        (uint8_t)((angle_raw >> 8) & 0xFF),
        0xFF
    };
    return build_ap_command(sa, target, 0x0A, payload, seq, out);
}

size_t ac42_build_tack(uint8_t sa, uint8_t target, ac42_side_t side,
                       uint8_t seq, n2k_frame_t out[2])
{
    /* For a tack, 0x02 = port, 0x03 = starboard. */
    const uint8_t payload[6] = {
        0x11,
        0x00,
        (uint8_t)(side == AC42_SIDE_STBD ? 0x03 : 0x02),
        0xFF,
        0xFF,
        0xFF
    };
    return build_ap_command(sa, target, 0x0A, payload, seq, out);
}

size_t ac42_build_response(uint8_t sa, uint8_t target,
                           ac42_response_set_t set,
                           uint8_t seq, n2k_frame_t out[2])
{
    uint8_t event;
    switch (set) {
    case AC42_RESPONSE_LOW:  event = 0x08; break;
    case AC42_RESPONSE_HIGH: event = 0x07; break;
    case AC42_RESPONSE_AUTO: event = 0x46; break;
    default: return 0;
    }
    const uint8_t payload[6] = {event, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
    return build_ap_command(sa, target, 0x0A, payload, seq, out);
}

size_t ac42_build_rudder(uint8_t sa, uint8_t target,
                         ac42_rudder_dir_t dir,
                         uint8_t seq, n2k_frame_t out[2])
{
    uint8_t direction;
    switch (dir) {
    case AC42_RUDDER_STBD: direction = 0x05; break;
    case AC42_RUDDER_PORT: direction = 0x04; break;
    case AC42_RUDDER_STOP: direction = 0xFF; break;
    default: return 0;
    }
    const uint8_t payload[6] = {0x0D, 0x00, direction, 0x00, 0x00, 0x00};
    return build_ap_command(sa, target, 0x02, payload, seq, out);
}

size_t ac42_build_key_query(uint8_t sa, uint8_t target, uint16_t key,
                            uint8_t seq, n2k_frame_t out[3])
{
    const uint8_t s = (uint8_t)((seq & 0x07u) << 5);
    const uint32_t id = n2k_make_id(3, AC42_PGN_KEY_VALUE, sa);

    out[0].id = id;
    out[0].dlc = 8;
    out[0].data[0] = (uint8_t)(s | 0u);
    out[0].data[1] = 0x0E;
    out[0].data[2] = 0x41;
    out[0].data[3] = 0x9F;
    out[0].data[4] = target;
    out[0].data[5] = 0xFF;
    out[0].data[6] = 0xFF;
    out[0].data[7] = 0xFF;

    out[1].id = id;
    out[1].dlc = 8;
    out[1].data[0] = (uint8_t)(s | 1u);
    out[1].data[1] = (uint8_t)(key & 0xFFu);
    out[1].data[2] = (uint8_t)((key >> 8) & 0xFFu);
    out[1].data[3] = 0x00;
    out[1].data[4] = 0x00;
    out[1].data[5] = 0xFF;
    out[1].data[6] = 0xFF;
    out[1].data[7] = 0xFF;

    out[2].id = id;
    out[2].dlc = 8;
    out[2].data[0] = (uint8_t)(s | 2u);
    for (size_t i = 1; i < 8; i++) {
        out[2].data[i] = 0xFF;
    }
    return 3;
}
