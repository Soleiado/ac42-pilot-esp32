#include "ac42_protocol.h"
#include "n2k_codec.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks = 0;

static void check_true(bool condition, const char *what)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        assert(condition);
    }
    checks++;
}

static void check_near(float actual, float expected, float tolerance,
                       const char *what)
{
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s (got %.4f, expected %.4f)\n",
                what, actual, expected);
        assert(false);
    }
    checks++;
}

static void expect_frame(const n2k_frame_t *f, uint32_t id,
                         const uint8_t data[8], const char *what)
{
    if (f->id != id || f->dlc != 8 || memcmp(f->data, data, 8) != 0) {
        fprintf(stderr, "FAIL: %s\n", what);
        fprintf(stderr, "  id=%08X (expected %08X), dlc=%u\n",
                (unsigned)f->id, (unsigned)id, f->dlc);
        fprintf(stderr, "  got     :");
        for (int i = 0; i < 8; i++) fprintf(stderr, " %02X", f->data[i]);
        fprintf(stderr, "\n  expected:");
        for (int i = 0; i < 8; i++) fprintf(stderr, " %02X", data[i]);
        fprintf(stderr, "\n");
        assert(false);
    }
    checks++;
}

static void feed(n2k_frame_t *frame, ac42_state_t *st, uint32_t now_ms)
{
    ac42_state_on_frame(st, frame, now_ms);
}

static void feed_single(ac42_state_t *st, uint32_t pgn, uint8_t src,
                        const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    n2k_frame_t f = {0};
    f.id = n2k_make_id(3, pgn, src);
    f.dlc = dlc;
    memcpy(f.data, data, dlc);
    feed(&f, st, now_ms);
}

static void feed_fast(ac42_state_t *st, uint32_t pgn, uint8_t src,
                      const uint8_t *payload, size_t len, uint32_t now_ms)
{
    const uint8_t seq = 0;
    size_t offset = 0;
    uint8_t frame_index = 0;

    while (offset < len) {
        n2k_frame_t f = {0};
        f.id = n2k_make_id(2, pgn, src);
        f.dlc = 8;
        f.data[0] = (uint8_t)((seq << 5) | frame_index);
        if (frame_index == 0) {
            f.data[1] = (uint8_t)len;
            const size_t n = len < 6 ? len : 6;
            memcpy(&f.data[2], payload, n);
            offset += n;
            if (n < 6) {
                for (size_t i = 2 + n; i < 8; i++) f.data[i] = 0xFF;
            }
        } else {
            const size_t n = (len - offset) < 7 ? (len - offset) : 7;
            memcpy(&f.data[1], payload + offset, n);
            offset += n;
            for (size_t i = 1 + n; i < 8; i++) f.data[i] = 0xFF;
        }
        feed(&f, st, now_ms);
        frame_index++;
    }
}

static void test_builders(void)
{
    const uint8_t sa = 0x07;
    const uint8_t target = 0x02;
    const uint32_t cmd_id = n2k_make_id(2, 130850, sa);
    const uint32_t key_id = n2k_make_id(3, 130845, 0x06);

    n2k_frame_t f[3];
    size_t n;

    n = ac42_build_mode(sa, target, AC42_MODE_AUTO, 0, f);
    check_true(n == 2, "mode auto: 2 frames");
    {
        const uint8_t f0[8] = {0x00, 0x0C, 0x41, 0x9F, 0x02, 0xFF, 0xFF, 0x0A};
        const uint8_t f1[8] = {0x01, 0x09, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[0], cmd_id, f0, "mode auto frame 0");
        expect_frame(&f[1], cmd_id, f1, "mode auto frame 1");
    }

    n = ac42_build_mode(sa, target, AC42_MODE_WIND, 0, f);
    check_true(n == 2, "mode wind: 2 frames");
    {
        const uint8_t f1[8] = {0x01, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "mode wind frame 1");
    }

    n = ac42_build_mode(sa, target, AC42_MODE_STANDBY, 0, f);
    check_true(n == 2, "mode standby: 2 frames");
    {
        const uint8_t f1[8] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "mode standby frame 1");
    }

    /* Validated plugin vectors: Wind bear 1 deg = 1A 00 02 AE 00 FF. */
    n = ac42_build_course(sa, target, AC42_SIDE_STBD, 1, true, 0, f);
    check_true(n == 2, "course wind stbd 1: 2 frames");
    {
        const uint8_t f1[8] = {0x01, 0x1A, 0x00, 0x02, 0xAE, 0x00, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "course wind stbd 1 frame 1");
    }

    n = ac42_build_course(sa, target, AC42_SIDE_PORT, 1, true, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x1A, 0x00, 0x03, 0xAE, 0x00, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "course wind port 1 frame 1");
    }

    n = ac42_build_course(sa, target, AC42_SIDE_STBD, 1, false, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x1A, 0x00, 0x03, 0xAE, 0x00, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "course auto stbd 1 frame 1");
    }

    n = ac42_build_course(sa, target, AC42_SIDE_PORT, 10, false, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x1A, 0x00, 0x02, 0xD1, 0x06, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "course auto port 10 frame 1");
    }

    n = ac42_build_course(sa, target, AC42_SIDE_STBD, 5, false, 0, f);
    check_true(n == 0, "course rejects unsupported angle");

    n = ac42_build_tack(sa, target, AC42_SIDE_STBD, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x11, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "tack stbd frame 1");
    }
    n = ac42_build_tack(sa, target, AC42_SIDE_PORT, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x11, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "tack port frame 1");
    }

    n = ac42_build_response(sa, target, AC42_RESPONSE_LOW, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "response low frame 1");
    }
    n = ac42_build_response(sa, target, AC42_RESPONSE_HIGH, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x07, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "response high frame 1");
    }
    n = ac42_build_response(sa, target, AC42_RESPONSE_AUTO, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x46, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "response auto frame 1");
    }

    n = ac42_build_rudder(sa, target, AC42_RUDDER_STBD, 0, f);
    {
        const uint8_t f0[8] = {0x00, 0x0C, 0x41, 0x9F, 0x02, 0xFF, 0xFF, 0x02};
        const uint8_t f1[8] = {0x01, 0x0D, 0x00, 0x05, 0x00, 0x00, 0x00, 0xFF};
        expect_frame(&f[0], cmd_id, f0, "rudder stbd frame 0");
        expect_frame(&f[1], cmd_id, f1, "rudder stbd frame 1");
    }
    n = ac42_build_rudder(sa, target, AC42_RUDDER_PORT, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x0D, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "rudder port frame 1");
    }
    n = ac42_build_rudder(sa, target, AC42_RUDDER_STOP, 0, f);
    {
        const uint8_t f1[8] = {0x01, 0x0D, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF};
        expect_frame(&f[1], cmd_id, f1, "rudder stop frame 1");
    }

    /* Exact plugin vector for key query, with sequence group 1. */
    n = ac42_build_key_query(0x06, 0x02, AC42_KEY_RESPONSE_LOW, 1, f);
    check_true(n == 3, "key query: 3 frames");
    {
        const uint8_t f0[8] = {0x20, 0x0E, 0x41, 0x9F, 0x02, 0xFF, 0xFF, 0xFF};
        const uint8_t f1[8] = {0x21, 0x19, 0x11, 0x00, 0x00, 0xFF, 0xFF, 0xFF};
        const uint8_t f2[8] = {0x22, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        expect_frame(&f[0], key_id, f0, "key query frame 0");
        expect_frame(&f[1], key_id, f1, "key query frame 1");
        expect_frame(&f[2], key_id, f2, "key query frame 2");
    }
}

static void test_parser(void)
{
    ac42_state_t st;
    ac42_state_init(&st);

    /* Controller heartbeat from a B&G display at address 7. */
    {
        const uint8_t p[8] = {0x41, 0x9F, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
        feed_single(&st, AC42_PGN_DEVICE_STATUS, 7, p, 8, 1000);
        uint8_t addr = 0;
        check_true(ac42_best_controller(&st, 1000, 5000, &addr), "controller detected");
        check_true(addr == 7, "controller address = 7");
        check_true(ac42_best_controller(&st, 6000, 5000, &addr), "controller still fresh");
        check_true(!ac42_best_controller(&st, 6001, 5000, &addr), "controller stale");
    }

    /* AC42 Device Status: automatic = engaged. */
    {
        const uint8_t p[8] = {0x41, 0x9F, 0x00, 0x02, 0x10, 0x00, 0x00, 0x00};
        feed_single(&st, AC42_PGN_DEVICE_STATUS, 2, p, 8, 1200);
        check_true(st.ap_valid && st.ap_addr == 2, "AP address = 2");
        check_true(st.engaged_valid && st.engaged, "AP engaged");
    }

    /* Pilot Mode: Wind + response automatic + response high. */
    {
        const uint8_t p[8] = {0x41, 0x9F, 0x00, 0x0A, 0x06, 0x04, 0x00, 0x00};
        feed_single(&st, AC42_PGN_DEVICE_STATUS, 2, p, 8, 1300);
        const ac42_mode_t mode = ac42_effective_mode(&st, 1300, 5000);
        check_true(mode == AC42_MODE_WIND, "pilot mode = Wind");
        check_true(st.response_valid && st.response_high, "response high flag");
        check_true(st.response_valid && st.response_auto, "response auto flag");
    }

    /* 65340 Mode State: engaged Heading (Auto). */
    {
        const uint8_t p[8] = {0x41, 0x9F, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00};
        feed_single(&st, AC42_PGN_MODE_STATE, 2, p, 8, 1400);
        check_true(ac42_effective_mode(&st, 1400, 5000) == AC42_MODE_AUTO,
                   "mode state = Auto");
        check_true(ac42_effective_engaged(&st, 1400, 5000),
                   "mode state engaged");
    }

    /* 65341: wind mode, 45 deg commanded apparent wind angle. */
    {
        const uint16_t raw = (uint16_t)lroundf(45.0f * 0.017453292519943295f / 0.0001f);
        const uint8_t p[8] = {0x41, 0x9F, 0x00, 0x00, 0x03, 0x00,
                              (uint8_t)(raw & 0xFF), (uint8_t)(raw >> 8)};
        feed_single(&st, AC42_PGN_AP_ANGLE, 2, p, 8, 1500);
        check_true(st.wind_target_deg.valid, "wind target valid");
        check_near(st.wind_target_deg.value, 45.0f, 0.2f, "wind target 45 deg");
    }

    /* 127237: heading-to-steer 90 deg magnetic. */
    {
        uint8_t p[21] = {0};
        p[1] = 0x40; /* reference = magnetic (bits 14-15) */
        const uint16_t target = (uint16_t)lroundf(90.0f * 0.017453292519943295f / 0.0001f);
        p[5] = (uint8_t)(target & 0xFF);
        p[6] = (uint8_t)(target >> 8);
        feed_fast(&st, AC42_PGN_HEADING_TRACK, 2, p, sizeof(p), 1600);
        check_true(st.target_mag_deg.valid, "target heading valid");
        check_near(st.target_mag_deg.value, 90.0f, 0.2f, "target heading 90 deg");
    }

    /* 127250: heading 270 magnetic, variation 10 deg. */
    {
        const uint16_t heading = (uint16_t)lroundf(270.0f * 0.017453292519943295f / 0.0001f);
        const int16_t variation = (int16_t)lroundf(10.0f * 0.017453292519943295f / 0.0001f);
        const uint8_t p[8] = {0x00,
                              (uint8_t)(heading & 0xFF), (uint8_t)(heading >> 8),
                              0x00, 0x00,
                              (uint8_t)((uint16_t)variation & 0xFF),
                              (uint8_t)((uint16_t)variation >> 8),
                              0x01};
        feed_single(&st, AC42_PGN_VESSEL_HEADING, 2, p, 8, 1700);
        check_near(st.heading_mag_deg.value, 270.0f, 0.2f, "heading magnetic 270");
        check_near(st.heading_true_deg.value, 280.0f, 0.2f, "heading true 280");
    }

    /* 130306: apparent wind 30 deg, 10 m/s. */
    {
        const uint16_t angle = (uint16_t)lroundf(30.0f * 0.017453292519943295f / 0.0001f);
        const uint16_t speed = 1000;
        const uint8_t p[8] = {0x00,
                              (uint8_t)(speed & 0xFF), (uint8_t)(speed >> 8),
                              (uint8_t)(angle & 0xFF), (uint8_t)(angle >> 8),
                              0x02, 0x00, 0x00};
        feed_single(&st, AC42_PGN_WIND, 2, p, 8, 1800);
        check_near(st.awa_deg.value, 30.0f, 0.2f, "AWA 30 deg");
        check_near(st.aws_kn.value, 19.438f, 0.05f, "AWS 19.4 kn");
    }

    /* 129026: COG 180 true, SOG 5 m/s. */
    {
        const uint16_t cog = (uint16_t)lroundf(180.0f * 0.017453292519943295f / 0.0001f);
        const uint16_t sog = 500;
        const uint8_t p[8] = {0x00, 0x00,
                              (uint8_t)(cog & 0xFF), (uint8_t)(cog >> 8),
                              (uint8_t)(sog & 0xFF), (uint8_t)(sog >> 8),
                              0x00, 0x00};
        feed_single(&st, AC42_PGN_COG_SOG, 2, p, 8, 1900);
        check_near(st.cog_true_deg.value, 180.0f, 0.2f, "COG true 180");
        check_near(st.cog_mag_deg.value, 170.0f, 0.2f, "COG magnetic 170");
        check_near(st.sog_kn.value, 9.719f, 0.05f, "SOG 9.7 kn");
    }

    /* 127245: rudder position -10 deg. */
    {
        const int16_t rudder = (int16_t)lroundf(-10.0f * 0.017453292519943295f / 0.0001f);
        const uint8_t p[8] = {0x00, 0x00, 0x00, 0x00,
                              (uint8_t)((uint16_t)rudder & 0xFF),
                              (uint8_t)((uint16_t)rudder >> 8),
                              0x00, 0x00};
        feed_single(&st, AC42_PGN_RUDDER, 2, p, 8, 2000);
        check_near(st.rudder_deg.value, -10.0f, 0.2f, "rudder -10 deg");
    }

    /* 130845 response level: low set to 4 by the AC42. */
    {
        const uint8_t p[14] = {0x41, 0x9F, 0x02, 0xFF, 0xFF, 0xFF,
                               0x19, 0x11, 0x00, 0x02, 0x04, 0xFF, 0xFF, 0xFF};
        feed_fast(&st, AC42_PGN_KEY_VALUE, 2, p, sizeof(p), 2100);
        check_true(st.level_valid[AC42_LEVEL_LOW], "low level valid");
        check_true(st.levels[AC42_LEVEL_LOW] == 4, "low level = 4");
        check_true(ac42_levels_stale(&st, 2200, 60000), "levels stale when others missing");

        const uint8_t ph[14] = {0x41, 0x9F, 0x02, 0xFF, 0xFF, 0xFF,
                                0x1A, 0x11, 0x00, 0x02, 0x06, 0xFF, 0xFF, 0xFF};
        feed_fast(&st, AC42_PGN_KEY_VALUE, 2, ph, sizeof(ph), 2200);
        const uint8_t pw[14] = {0x41, 0x9F, 0x02, 0xFF, 0xFF, 0xFF,
                                0x1C, 0x11, 0x00, 0x02, 0x05, 0xFF, 0xFF, 0xFF};
        feed_fast(&st, AC42_PGN_KEY_VALUE, 2, pw, sizeof(pw), 2300);
        check_true(st.levels[AC42_LEVEL_HIGH] == 6, "high level = 6");
        check_true(st.levels[AC42_LEVEL_WIND] == 5, "wind level = 5");
        check_true(!ac42_levels_stale(&st, 2300, 60000), "levels fresh");
    }
}

static void test_effective_engaged(void)
{
    ac42_state_t st;
    ac42_state_init(&st);
    check_true(!ac42_effective_engaged(&st, 1000, 5000),
               "empty state not engaged");

    st.mode = AC42_MODE_AUTO;
    st.mode_valid = true;
    st.mode_ms = 1000;
    check_true(ac42_effective_engaged(&st, 2000, 5000),
               "engaged fallback from Auto mode");
    check_true(!ac42_effective_engaged(&st, 7001, 5000),
               "mode fallback expires");

    st.mode = AC42_MODE_STANDBY;
    st.mode_ms = 8000;
    check_true(!ac42_effective_engaged(&st, 8000, 5000),
               "standby is not engaged");

    st.engaged = true;
    st.engaged_valid = true;
    st.engaged_ms = 8000;
    check_true(ac42_effective_engaged(&st, 8000, 5000),
               "status overrides standby bit");
}

int main(void)
{
    test_builders();
    test_parser();
    test_effective_engaged();
    printf("All %d protocol checks passed.\n", checks);
    return 0;
}
