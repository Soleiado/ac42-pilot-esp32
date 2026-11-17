#include "ac42_controller.h"

#include "app_util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t kPilotStaleMs = 5000;
constexpr uint32_t kLevelsMaxAgeMs = 60000;
constexpr uint32_t kLevelsRetryMs = 15000;
constexpr uint32_t kCommandMinGapMs = 120;
constexpr uint32_t kNfuHardLimitMs = 8000;
constexpr uint8_t  kFallbackApAddress = 0x02;
}

void Ac42Controller::begin(N2kBus *bus, AppConfig *config, AppLog *log)
{
    bus_ = bus;
    config_ = config;
    log_ = log;
    if (mutex_ == nullptr) {
        mutex_ = xSemaphoreCreateMutex();
    }
    ac42_state_init(&state_);
    if (log_ != nullptr) {
        log_->add("Moteur AC42 prêt (écoute passive NMEA 2000)");
    }
}

Ac42CommandResult Ac42Controller::error(Ac42ResultCode code,
                                       const char *message) const
{
    Ac42CommandResult result;
    result.code = code;
    result.message = message != nullptr ? message : "";
    return result;
}

Ac42CommandResult Ac42Controller::success(const char *message,
                                         size_t frames) const
{
    Ac42CommandResult result;
    result.code = AC42_RESULT_OK;
    result.message = message != nullptr ? message : "";
    result.frames = frames;
    return result;
}

bool Ac42Controller::resolveSource(uint32_t now_ms, uint8_t &source)
{
    if (config_ != nullptr && config_->controller_override >= 0) {
        source = (uint8_t)config_->controller_override;
        return true;
    }

    uint8_t addr = 0;
    if (ac42_best_controller(&state_, now_ms, kPilotStaleMs, &addr)) {
        source = addr;
        return true;
    }
    return false;
}

uint8_t Ac42Controller::resolveTarget()
{
    if (config_ != nullptr && config_->ap_override >= 0) {
        return (uint8_t)config_->ap_override;
    }
    if (state_.ap_valid) {
        return state_.ap_addr;
    }
    return kFallbackApAddress;
}

Ac42CommandResult Ac42Controller::sendFramesLocked(
    const n2k_frame_t *frames, size_t count, const char *what, uint32_t now_ms)
{
    if (bus_ == nullptr || !bus_->ready()) {
        return error(AC42_RESULT_BUS_ERROR, "Bus CAN non prêt.");
    }

    for (size_t i = 0; i < count; i++) {
        if (!bus_->send(frames[i])) {
            seq_ = (uint8_t)((seq_ + 1u) & 0x07u);
            last_command_ms_ = now_ms;
            if (log_ != nullptr) {
                log_->add("%s: émission CAN échouée (trame %u/%u)",
                          what, (unsigned)(i + 1), (unsigned)count);
            }
            return error(AC42_RESULT_BUS_ERROR, "Émission CAN échouée.");
        }
    }

    seq_ = (uint8_t)((seq_ + 1u) & 0x07u);
    last_command_ms_ = now_ms;

    if (log_ != nullptr && strcmp(what, "NFU keepalive") != 0) {
        log_->add("%s: %u trame(s), SA=0x%02X cible=0x%02X",
                  what, (unsigned)count,
                  n2k_id_src(frames[0].id),
                  (unsigned)frames[0].data[4]);
    }
    return success(what, count);
}

void Ac42Controller::onFrame(const n2k_frame_t &frame, uint32_t now_ms)
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);

    ac42_state_on_frame(&state_, &frame, now_ms);

    if (state_.ap_valid && state_.ap_addr != last_logged_ap_) {
        last_logged_ap_ = state_.ap_addr;
        if (log_ != nullptr) {
            log_->add("AC42 détecté à l'adresse 0x%02X (modèle %u)",
                      state_.ap_addr, state_.ap_model);
        }
    }

    uint8_t controller = 0;
    if (ac42_best_controller(&state_, now_ms, kPilotStaleMs, &controller) &&
        controller != last_logged_controller_) {
        last_logged_controller_ = controller;
        if (log_ != nullptr) {
            log_->add("Pupitre/contrôleur actif détecté à l'adresse 0x%02X",
                      controller);
        }
    }

    xSemaphoreGive(mutex_);
}

Ac42Snapshot Ac42Controller::snapshot()
{
    Ac42Snapshot s;
    if (mutex_ == nullptr) {
        return s;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();

    s.mode = ac42_effective_mode(&state_, now, kPilotStaleMs);
    s.mode_fresh = state_.mode_valid &&
                   (uint32_t)(now - state_.mode_ms) <= kPilotStaleMs;
    s.engaged = ac42_effective_engaged(&state_, now, kPilotStaleMs);
    s.engaged_fresh = state_.engaged_valid &&
                      (uint32_t)(now - state_.engaged_ms) <= kPilotStaleMs;

    s.ap_addr = state_.ap_addr;
    s.ap_known = state_.ap_valid;
    s.ap_age_ms = state_.ap_valid ? (uint32_t)(now - state_.ap_ms) : 0;
    s.ap_present = state_.ap_valid && s.ap_age_ms <= kPilotStaleMs;
    s.ap_model = state_.ap_model;
    s.ap_override = config_ != nullptr ? config_->ap_override : -1;
    s.ap_forced = s.ap_override >= 0;

    s.controller_override = config_ != nullptr ? config_->controller_override : -1;
    s.controller_forced = s.controller_override >= 0;
    if (s.controller_forced) {
        s.controller_addr = (uint8_t)s.controller_override;
        s.controller_present = true;
        s.controller_age_ms = 0;
    } else {
        uint8_t addr = 0;
        if (ac42_best_controller(&state_, now, kPilotStaleMs, &addr)) {
            s.controller_addr = addr;
            s.controller_present = true;
            for (size_t i = 0; i < AC42_MAX_CONTROLLERS; i++) {
                if (state_.controller_ms[i] != 0 &&
                    state_.controller_addr[i] == addr) {
                    s.controller_age_ms =
                        (uint32_t)(now - state_.controller_ms[i]);
                    break;
                }
            }
        }
    }

    s.response_valid = s.engaged && state_.response_valid &&
                       (uint32_t)(now - state_.response_ms) <= kPilotStaleMs;
    s.response_high = state_.response_high;
    s.response_auto = state_.response_auto;
    s.mode_bits = state_.mode_bits;
    s.mode_bits_valid = state_.mode_bits_valid &&
                        (uint32_t)(now - state_.mode_bits_ms) <= kPilotStaleMs;
    memcpy(s.levels, state_.levels, sizeof(s.levels));
    memcpy(s.level_valid, state_.level_valid, sizeof(s.level_valid));
    s.levels_age_ms = state_.levels_ms != 0
                          ? (uint32_t)(now - state_.levels_ms)
                          : 0xFFFFFFFFu;

    s.target_mag_deg = state_.target_mag_deg;
    s.target_true_deg = state_.target_true_deg;
    s.wind_target_deg = state_.wind_target_deg;
    s.heading_mag_deg = state_.heading_mag_deg;
    s.heading_true_deg = state_.heading_true_deg;
    s.variation_deg = state_.variation_deg;
    s.awa_deg = state_.awa_deg;
    s.aws_kn = state_.aws_kn;
    s.twa_deg = state_.twa_deg;
    s.tws_kn = state_.tws_kn;
    s.twd_mag_deg = state_.twd_mag_deg;
    s.twd_true_deg = state_.twd_true_deg;
    s.sog_kn = state_.sog_kn;
    s.stw_kn = state_.stw_kn;
    s.cog_mag_deg = state_.cog_mag_deg;
    s.cog_true_deg = state_.cog_true_deg;
    s.depth_m = state_.depth_m;
    s.rudder_deg = state_.rudder_deg;
    s.commanded_rudder_deg = state_.commanded_rudder_deg;

    s.nfu_active = nfu_active_;
    s.nfu_dir = (uint8_t)nfu_dir_;
    s.nfu_elapsed_ms = nfu_active_
                           ? (uint32_t)(now - nfu_start_ms_)
                           : 0;
    s.rx_frames = state_.rx_frames;
    s.unsupported_frames = state_.unsupported_frames;

    xSemaphoreGive(mutex_);
    return s;
}

Ac42CommandResult Ac42Controller::setMode(ac42_mode_t mode)
{
    if (mutex_ == nullptr) {
        return error(AC42_RESULT_INVALID, "Moteur AC42 non initialisé.");
    }
    if (mode != AC42_MODE_STANDBY && mode != AC42_MODE_AUTO &&
        mode != AC42_MODE_WIND) {
        return error(AC42_RESULT_INVALID, "Mode non supporté.");
    }

    Ac42CommandResult result;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();
    if ((uint32_t)(now - last_command_ms_) < kCommandMinGapMs) {
        result = error(AC42_RESULT_RATE_LIMITED, "Trop de commandes rapprochées.");
    } else {
        uint8_t source = 0;
        if (!resolveSource(now, source)) {
            result = error(AC42_RESULT_NO_CONTROLLER,
                           "Aucun pupitre/contrôleur B&G actif détecté.");
        } else {
            const uint8_t target = resolveTarget();
            n2k_frame_t frames[2];
            const size_t count =
                ac42_build_mode(source, target, mode, seq_, frames);
            if (count == 0) {
                result = error(AC42_RESULT_INVALID, "Mode invalide.");
            } else {
                const char *label = mode == AC42_MODE_STANDBY
                                        ? "Mode Standby"
                                        : (mode == AC42_MODE_AUTO ? "Mode Auto"
                                                                  : "Mode Vent");
                result = sendFramesLocked(frames, count, label, now);
            }
        }
    }
    xSemaphoreGive(mutex_);
    return result;
}

Ac42CommandResult Ac42Controller::changeCourse(bool starboard, int degrees)
{
    if (mutex_ == nullptr) {
        return error(AC42_RESULT_INVALID, "Moteur AC42 non initialisé.");
    }
    if (degrees != 1 && degrees != 10) {
        return error(AC42_RESULT_INVALID, "Correction autorisée: 1 ou 10 degrés.");
    }

    Ac42CommandResult result;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();

    if ((uint32_t)(now - last_command_ms_) < kCommandMinGapMs) {
        result = error(AC42_RESULT_RATE_LIMITED, "Trop de commandes rapprochées.");
    } else {
        const ac42_mode_t mode =
            ac42_effective_mode(&state_, now, kPilotStaleMs);
        if (!ac42_effective_engaged(&state_, now, kPilotStaleMs)) {
            result = error(AC42_RESULT_NOT_ENGAGED,
                           "Le pilote est en standby: correction indisponible.");
        } else if (mode != AC42_MODE_AUTO && mode != AC42_MODE_WIND &&
                   mode != AC42_MODE_NO_DRIFT) {
            result = error(AC42_RESULT_INVALID,
                           "Correction de cap indisponible dans ce mode.");
        } else {
            uint8_t source = 0;
            if (!resolveSource(now, source)) {
                result = error(AC42_RESULT_NO_CONTROLLER,
                               "Aucun pupitre/contrôleur B&G actif détecté.");
            } else {
                const uint8_t target = resolveTarget();
                n2k_frame_t frames[2];
                const size_t count = ac42_build_course(
                    source, target,
                    starboard ? AC42_SIDE_STBD : AC42_SIDE_PORT,
                    degrees, mode == AC42_MODE_WIND, seq_, frames);
                if (count == 0) {
                    result = error(AC42_RESULT_INVALID, "Correction invalide.");
                } else {
                    char label[48];
                    snprintf(label, sizeof(label), "Correction %s %d°",
                             starboard ? "tribord" : "bâbord", degrees);
                    result = sendFramesLocked(frames, count, label, now);
                }
            }
        }
    }
    xSemaphoreGive(mutex_);
    return result;
}

Ac42CommandResult Ac42Controller::tack(bool starboard)
{
    if (mutex_ == nullptr) {
        return error(AC42_RESULT_INVALID, "Moteur AC42 non initialisé.");
    }

    Ac42CommandResult result;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();

    if ((uint32_t)(now - last_command_ms_) < kCommandMinGapMs) {
        result = error(AC42_RESULT_RATE_LIMITED, "Trop de commandes rapprochées.");
    } else if (!ac42_effective_engaged(&state_, now, kPilotStaleMs)) {
        result = error(AC42_RESULT_NOT_ENGAGED,
                       "Le pilote est en standby: virement indisponible.");
    } else if (ac42_effective_mode(&state_, now, kPilotStaleMs) ==
               AC42_MODE_NAV) {
        result = error(AC42_RESULT_INVALID,
                       "Virement indisponible en mode Nav.");
    } else {
        uint8_t source = 0;
        if (!resolveSource(now, source)) {
            result = error(AC42_RESULT_NO_CONTROLLER,
                           "Aucun pupitre/contrôleur B&G actif détecté.");
        } else {
            const uint8_t target = resolveTarget();
            n2k_frame_t frames[2];
            const size_t count = ac42_build_tack(
                source, target, starboard ? AC42_SIDE_STBD : AC42_SIDE_PORT,
                seq_, frames);
            if (count == 0) {
                result = error(AC42_RESULT_INVALID, "Virement invalide.");
            } else {
                result = sendFramesLocked(
                    frames, count,
                    starboard ? "Virement tribord" : "Virement bâbord", now);
            }
        }
    }
    xSemaphoreGive(mutex_);
    return result;
}

Ac42CommandResult Ac42Controller::setResponse(ac42_response_set_t set)
{
    if (mutex_ == nullptr) {
        return error(AC42_RESULT_INVALID, "Moteur AC42 non initialisé.");
    }

    Ac42CommandResult result;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();

    if ((uint32_t)(now - last_command_ms_) < kCommandMinGapMs) {
        result = error(AC42_RESULT_RATE_LIMITED, "Trop de commandes rapprochées.");
    } else {
        uint8_t source = 0;
        if (!resolveSource(now, source)) {
            result = error(AC42_RESULT_NO_CONTROLLER,
                           "Aucun pupitre/contrôleur B&G actif détecté.");
        } else {
            const uint8_t target = resolveTarget();
            n2k_frame_t frames[2];
            const size_t count =
                ac42_build_response(source, target, set, seq_, frames);
            if (count == 0) {
                result = error(AC42_RESULT_INVALID, "Réglage invalide.");
            } else {
                char label[48];
                snprintf(label, sizeof(label), "Réponse %s",
                         ac42_response_set_name(set));
                result = sendFramesLocked(frames, count, label, now);
            }
        }
    }
    xSemaphoreGive(mutex_);
    return result;
}

Ac42CommandResult Ac42Controller::rudder(ac42_rudder_dir_t direction)
{
    if (mutex_ == nullptr) {
        return error(AC42_RESULT_INVALID, "Moteur AC42 non initialisé.");
    }

    Ac42CommandResult result;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint32_t now = app_now_ms();

    if (direction != AC42_RUDDER_STOP &&
        ac42_effective_engaged(&state_, now, kPilotStaleMs)) {
        result = error(AC42_RESULT_NOT_STANDBY,
                       "Barre directe uniquement en standby.");
    } else {
        uint8_t source = 0;
        uint8_t target = 0;

        if (direction == AC42_RUDDER_STOP && nfu_active_) {
            /* Use the addresses captured at the start: even if the controller
             * heartbeat is lost, the safest action is still to stop the pump. */
            source = nfu_source_;
            target = nfu_target_;
        } else if (!resolveSource(now, source)) {
            result = error(AC42_RESULT_NO_CONTROLLER,
                           "Aucun pupitre/contrôleur B&G actif détecté.");
        } else {
            target = resolveTarget();
        }

        if (result.code == AC42_RESULT_OK) {
            n2k_frame_t frames[2];
            const size_t count =
                ac42_build_rudder(source, target, direction, seq_, frames);
            if (count == 0) {
                result = error(AC42_RESULT_INVALID, "Commande de barre invalide.");
            } else {
                const char *label = direction == AC42_RUDDER_STOP
                                        ? "Barre stop"
                                        : (direction == AC42_RUDDER_STBD
                                               ? "Barre tribord"
                                               : "Barre bâbord");
                const bool keepalive =
                    direction != AC42_RUDDER_STOP && nfu_active_ &&
                    nfu_dir_ == direction;
                result = sendFramesLocked(frames, count,
                                          keepalive ? "NFU keepalive" : label,
                                          now);
                if (result.ok()) {
                    if (direction == AC42_RUDDER_STOP) {
                        nfu_active_ = false;
                    } else {
                        if (!nfu_active_ || nfu_dir_ != direction) {
                            nfu_start_ms_ = now;
                            nfu_retry_at_ms_ = 0;
                        }
                        nfu_active_ = true;
                        nfu_dir_ = direction;
                        nfu_source_ = source;
                        nfu_target_ = target;
                        nfu_last_cmd_ms_ = now;
                    }
                }
            }
        }
    }
    xSemaphoreGive(mutex_);
    return result;
}

void Ac42Controller::tickLocked(uint32_t now_ms)
{
    ac42_state_tick(&state_, now_ms);

    const bool engaged = ac42_effective_engaged(&state_, now_ms, kPilotStaleMs);

    if (nfu_active_) {
        const bool timed_out =
            (uint32_t)(now_ms - nfu_last_cmd_ms_) >
                (config_ != nullptr ? config_->rudder_timeout_ms : 1000u);
        const bool hard_limit =
            (uint32_t)(now_ms - nfu_start_ms_) > kNfuHardLimitMs;
        if ((engaged || timed_out || hard_limit) &&
            (int32_t)(now_ms - nfu_retry_at_ms_) >= 0) {
            n2k_frame_t frames[2];
            const size_t count = ac42_build_rudder(
                nfu_source_, nfu_target_, AC42_RUDDER_STOP, seq_, frames);
            bool stopped = (count == 0);
            if (count > 0) {
                const Ac42CommandResult stop_result =
                    sendFramesLocked(frames, count, "Barre stop sécurité",
                                     now_ms);
                stopped = stop_result.ok() || bus_ == nullptr || !bus_->ready();
            }
            if (stopped) {
                nfu_active_ = false;
                if (log_ != nullptr) {
                    log_->add("Barre directe: arrêt automatique (%s)",
                              engaged ? "pilote engagé"
                                      : (hard_limit ? "durée maximale"
                                                    : "délai sans keepalive"));
                }
            } else {
                nfu_retry_at_ms_ = now_ms + 500u;
                if (log_ != nullptr) {
                    log_->add("Barre directe: échec de l'arrêt, nouvelle tentative");
                }
            }
        }
    }

    const bool ap_usable = state_.ap_valid ||
                           (config_ != nullptr && config_->ap_override >= 0);
    if (!nfu_active_ && engaged && ap_usable &&
        ac42_levels_stale(&state_, now_ms, kLevelsMaxAgeMs) &&
        (last_key_query_ms_ == 0 ||
         (uint32_t)(now_ms - last_key_query_ms_) > kLevelsRetryMs)) {
        uint8_t source = 0;
        if (resolveSource(now_ms, source)) {
            const uint8_t target = resolveTarget();
            last_key_query_ms_ = now_ms;
            const uint16_t keys[3] = {
                AC42_KEY_RESPONSE_LOW,
                AC42_KEY_RESPONSE_HIGH,
                AC42_KEY_RESPONSE_WIND,
            };
            for (size_t i = 0; i < 3; i++) {
                n2k_frame_t frames[3];
                const size_t count =
                    ac42_build_key_query(source, target, keys[i], seq_, frames);
                if (count > 0) {
                    (void)sendFramesLocked(frames, count, "Lecture niveaux",
                                           now_ms);
                }
            }
        }
    }
}

void Ac42Controller::tick()
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    tickLocked(app_now_ms());
    xSemaphoreGive(mutex_);
}
