#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ac42_protocol.h"
#include "app_config.h"
#include "app_log.h"
#include "n2k_bus.h"

enum Ac42ResultCode {
    AC42_RESULT_OK = 0,
    AC42_RESULT_NO_CONTROLLER,
    AC42_RESULT_NOT_ENGAGED,
    AC42_RESULT_NOT_STANDBY,
    AC42_RESULT_BUS_ERROR,
    AC42_RESULT_RATE_LIMITED,
    AC42_RESULT_INVALID,
};

struct Ac42CommandResult {
    Ac42ResultCode code = AC42_RESULT_OK;
    String message;
    size_t frames = 0;

    bool ok() const { return code == AC42_RESULT_OK; }

    int httpStatus() const
    {
        switch (code) {
        case AC42_RESULT_OK:            return 200;
        case AC42_RESULT_NO_CONTROLLER: return 409;
        case AC42_RESULT_NOT_ENGAGED:   return 409;
        case AC42_RESULT_NOT_STANDBY:   return 409;
        case AC42_RESULT_BUS_ERROR:     return 502;
        case AC42_RESULT_RATE_LIMITED:  return 429;
        default:                        return 400;
        }
    }
};

struct Ac42Snapshot {
    ac42_mode_t mode = AC42_MODE_UNKNOWN;
    bool mode_fresh = false;
    bool engaged = false;
    bool engaged_fresh = false;

    uint8_t  ap_addr = 0;
    bool     ap_known = false;
    bool     ap_present = false;
    bool     ap_forced = false;
    int      ap_override = -1;
    uint32_t ap_age_ms = 0;
    uint8_t  ap_model = 0;

    uint8_t  controller_addr = 0;
    bool     controller_present = false;
    bool     controller_forced = false;
    int      controller_override = -1;
    uint32_t controller_age_ms = 0;

    bool     response_valid = false;
    bool     response_high = false;
    bool     response_auto = false;
    uint16_t mode_bits = 0;
    bool     mode_bits_valid = false;

    uint8_t  levels[AC42_LEVEL_COUNT] = {};
    bool     level_valid[AC42_LEVEL_COUNT] = {};
    uint32_t levels_age_ms = 0;

    ac42_value_t target_mag_deg;
    ac42_value_t target_true_deg;
    ac42_value_t wind_target_deg;
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

    bool     nfu_active = false;
    uint8_t  nfu_dir = 0;
    uint32_t nfu_elapsed_ms = 0;
    uint32_t rx_frames = 0;
    uint32_t unsupported_frames = 0;
};

class Ac42Controller {
public:
    void begin(N2kBus *bus, AppConfig *config, AppLog *log);
    void onFrame(const n2k_frame_t &frame, uint32_t now_ms);
    void tick();
    Ac42Snapshot snapshot();

    Ac42CommandResult setMode(ac42_mode_t mode);
    Ac42CommandResult changeCourse(bool starboard, int degrees);
    Ac42CommandResult tack(bool starboard);
    Ac42CommandResult rudder(ac42_rudder_dir_t direction);
    Ac42CommandResult setResponse(ac42_response_set_t set);

private:
    bool resolveSource(uint32_t now_ms, uint8_t &source);
    uint8_t resolveTarget();
    Ac42CommandResult sendFramesLocked(const n2k_frame_t *frames, size_t count,
                                       const char *what, uint32_t now_ms);
    Ac42CommandResult error(Ac42ResultCode code, const char *message) const;
    Ac42CommandResult success(const char *message, size_t frames) const;
    void tickLocked(uint32_t now_ms);

    N2kBus *bus_ = nullptr;
    AppConfig *config_ = nullptr;
    AppLog *log_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;

    ac42_state_t state_ = {};
    uint8_t seq_ = 0;
    uint32_t last_command_ms_ = 0;
    uint32_t last_key_query_ms_ = 0;

    bool nfu_active_ = false;
    ac42_rudder_dir_t nfu_dir_ = AC42_RUDDER_STOP;
    uint32_t nfu_start_ms_ = 0;
    uint32_t nfu_last_cmd_ms_ = 0;
    uint32_t nfu_retry_at_ms_ = 0;
    uint8_t nfu_source_ = 0;
    uint8_t nfu_target_ = 0;

    uint8_t last_logged_ap_ = 0xFF;
    uint8_t last_logged_controller_ = 0xFF;
};
