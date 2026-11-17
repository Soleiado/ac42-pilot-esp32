#include "web_server.h"

#include "app_util.h"
#include "generated/web_asset.h"
#include "version.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void addValue(JsonObject obj, const char *key, const ac42_value_t &value,
              uint32_t now_ms, uint32_t max_age_ms = 5000)
{
    if (ac42_value_fresh(&value, now_ms, max_age_ms)) {
        obj[key] = roundf(value.value * 10.0f) / 10.0f;
    } else {
        obj[key] = nullptr;
    }
}

void addAddress(JsonObject obj, const char *key, bool known, uint8_t address)
{
    if (known) {
        obj[key] = address;
    } else {
        obj[key] = nullptr;
    }
}

void addHexAddress(JsonObject obj, const char *key, bool known, uint8_t address)
{
    if (known) {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", address);
        obj[key] = String(buf);
    } else {
        obj[key] = nullptr;
    }
}

} // namespace

void WebServerApp::begin(Ac42Controller *controller, WifiManager *wifi,
                         AppConfig *config, AppLog *log, N2kBus *bus)
{
    controller_ = controller;
    wifi_ = wifi;
    config_ = config;
    log_ = log;
    bus_ = bus;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/index.html", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/favicon.ico", HTTP_GET, [this]() { handleFavicon(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/settings", HTTP_GET, [this]() { handleSettingsGet(); });
    server_.on("/api/settings", HTTP_POST, [this]() { handleSettingsPost(); });
    server_.on("/api/mode", HTTP_POST, [this]() { handleMode(); });
    server_.on("/api/course", HTTP_POST, [this]() { handleCourse(); });
    server_.on("/api/tack", HTTP_POST, [this]() { handleTack(); });
    server_.on("/api/rudder", HTTP_POST, [this]() { handleRudder(); });
    server_.on("/api/response", HTTP_POST, [this]() { handleResponse(); });
    server_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
    server_.on("/api/log", HTTP_GET, [this]() { handleLog(); });
    server_.onNotFound([this]() { handleNotFound(); });
    static const char *headerKeys[] = {"Content-Type"};
    server_.collectHeaders(headerKeys, 1);
    server_.begin();

    if (log_ != nullptr) {
        const WifiStatus wifi = wifi_ != nullptr ? wifi_->status() : WifiStatus();
        if (wifi.ap_started && wifi.ap_ip.length() > 0) {
            log_->add("Interface web: http://%s/ (%s)",
                      wifi.ap_ip.c_str(), wifi.mode.c_str());
        } else if (wifi.sta_connected) {
            log_->add("Interface web: http://%s/", wifi.sta_ip.c_str());
        } else {
            log_->add("Interface web prête (en attente du réseau Wi-Fi)");
        }
    }
}

void WebServerApp::loop()
{
    server_.handleClient();
    if (reboot_pending_ && (int32_t)(millis() - reboot_at_ms_) >= 0) {
        reboot_pending_ = false;
        if (log_ != nullptr) {
            log_->add("Redémarrage demandé");
        }
        delay(50);
        ESP.restart();
    }
}

void WebServerApp::handleRoot()
{
    server_.sendHeader("Cache-Control", "no-store, max-age=0");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("X-Frame-Options", "DENY");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
        "img-src 'self' data:; connect-src 'self'; base-uri 'none'; form-action 'self'");
    server_.send_P(200, "text/html; charset=utf-8",
                   (PGM_P)WEB_INDEX_HTML, WEB_INDEX_HTML_LEN);
}

void WebServerApp::handleFavicon()
{
    server_.send(204, "text/plain", "");
}

void WebServerApp::sendJson(int code, const String &payload)
{
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.send(code, "application/json; charset=utf-8", payload);
}

void WebServerApp::sendError(int code, const String &message)
{
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    sendJson(code, out);
}

bool WebServerApp::ensureJson()
{
    const String content_type = server_.header("Content-Type");
    String lowered = content_type;
    lowered.toLowerCase();
    if (lowered.indexOf("application/json") >= 0) {
        return true;
    }
    if (!content_type.isEmpty()) {
        if (log_ != nullptr) {
            log_->add("Content-Type refusé: '%s'", content_type.c_str());
        }
        sendError(415, "Content-Type application/json requis.");
        return false;
    }

    /* Some Arduino WebServer builds do not expose request headers unless they
     * are explicitly collected.  The settings UI always sends a JSON body, so
     * accept a body that is visibly a JSON object as a fallback.  This keeps
     * the page usable while the header collection is being diagnosed. */
    const String body = requestBody();
    size_t start = 0;
    while (start < body.length() &&
           (body[start] == ' ' || body[start] == '\r' || body[start] == '\n' ||
            body[start] == '\t')) {
        start++;
    }
    if (start < body.length() && body[start] == '{') {
        if (log_ != nullptr) {
            log_->add("Content-Type absent, JSON détecté dans le corps");
        }
        return true;
    }

    sendError(415, "Content-Type application/json requis.");
    return false;
}

String WebServerApp::requestBody()
{
    return server_.arg("plain");
}

void WebServerApp::sendCommandResult(const Ac42CommandResult &result)
{
    JsonDocument doc;
    doc["ok"] = result.ok();
    doc["message"] = result.message;
    doc["frames"] = (unsigned)result.frames;
    if (!result.ok()) {
        doc["error"] = result.message;
    }
    String out;
    serializeJson(doc, out);
    sendJson(result.httpStatus(), out);
}

void WebServerApp::handleStatus()
{
    if (controller_ == nullptr || config_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }

    const uint32_t now = app_now_ms();
    const Ac42Snapshot snap = controller_->snapshot();
    const CanStats can = bus_ != nullptr ? bus_->stats() : CanStats();
    const WifiStatus wifi = wifi_ != nullptr ? wifi_->status() : WifiStatus();

    JsonDocument doc;
    doc["ok"] = true;
    doc["name"] = AC42_FW_NAME;
    doc["version"] = AC42_FW_VERSION;
    doc["uptime_s"] = (unsigned long)(millis() / 1000UL);
    doc["free_heap"] = ESP.getFreeHeap();
    doc["simulated"] = false;

    JsonObject pilot = doc["pilot"].to<JsonObject>();
    pilot["mode"] = ac42_mode_name(snap.mode);
    pilot["mode_code"] = (int)snap.mode;
    pilot["mode_fresh"] = snap.mode_fresh;
    pilot["engaged"] = snap.engaged;
    pilot["engaged_fresh"] = snap.engaged_fresh;
    pilot["nfu_active"] = snap.nfu_active;
    pilot["nfu_elapsed_ms"] = snap.nfu_elapsed_ms;
    if (snap.mode_bits_valid) {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%04X", snap.mode_bits);
        pilot["mode_bits"] = String(buf);
    } else {
        pilot["mode_bits"] = nullptr;
    }

    JsonObject response = pilot["response"].to<JsonObject>();
    response["valid"] = snap.response_valid;
    if (snap.response_valid) {
        response["set"] = snap.response_high ? "high" : "low";
        response["auto"] = snap.response_auto;
        const size_t idx = snap.mode == AC42_MODE_WIND
                               ? AC42_LEVEL_WIND
                               : (snap.response_high ? AC42_LEVEL_HIGH
                                                     : AC42_LEVEL_LOW);
        if (snap.level_valid[idx]) {
            response["level"] = snap.levels[idx];
        } else {
            response["level"] = nullptr;
        }
    } else {
        response["set"] = nullptr;
        response["auto"] = nullptr;
        response["level"] = nullptr;
    }
    auto addLevel = [&](const char *key, size_t idx) {
        if (snap.level_valid[idx]) {
            response[key] = snap.levels[idx];
        } else {
            response[key] = nullptr;
        }
    };
    addLevel("low", AC42_LEVEL_LOW);
    addLevel("high", AC42_LEVEL_HIGH);
    addLevel("wind", AC42_LEVEL_WIND);

    const bool target_mag = ac42_value_fresh(&snap.target_mag_deg, now, 5000);
    const bool target_true = ac42_value_fresh(&snap.target_true_deg, now, 5000);
    addValue(pilot, "target_mag_deg", snap.target_mag_deg, now);
    addValue(pilot, "target_true_deg", snap.target_true_deg, now);
    if (target_mag) {
        pilot["target_deg"] = roundf(snap.target_mag_deg.value * 10.0f) / 10.0f;
        pilot["target_ref"] = "M";
    } else if (target_true) {
        pilot["target_deg"] = roundf(snap.target_true_deg.value * 10.0f) / 10.0f;
        pilot["target_ref"] = "T";
    } else {
        pilot["target_deg"] = nullptr;
        pilot["target_ref"] = nullptr;
    }
    addValue(pilot, "wind_target_deg", snap.wind_target_deg, now);
    if (ac42_value_fresh(&snap.wind_target_deg, now, 5000)) {
        const float v = snap.wind_target_deg.value;
        pilot["wind_target_side"] =
            v > 0.5f ? "stbd" : (v < -0.5f ? "port" : "none");
    } else {
        pilot["wind_target_side"] = nullptr;
    }

    JsonObject ap = doc["ap"].to<JsonObject>();
    ap["known"] = snap.ap_known;
    ap["present"] = snap.ap_present;
    ap["forced"] = snap.ap_forced;
    ap["override"] = AppConfig::addressToString(snap.ap_override);
    ap["age_ms"] = snap.ap_known ? snap.ap_age_ms : 0;
    addAddress(ap, "address", snap.ap_known, snap.ap_addr);
    addHexAddress(ap, "address_hex", snap.ap_known, snap.ap_addr);
    if (snap.ap_forced) {
        ap["effective_address"] = snap.ap_override;
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", snap.ap_override);
        ap["effective_address_hex"] = String(buf);
    } else if (snap.ap_known) {
        ap["effective_address"] = snap.ap_addr;
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", snap.ap_addr);
        ap["effective_address_hex"] = String(buf);
    } else {
        ap["effective_address"] = 0x02;
        ap["effective_address_hex"] = "0x02";
    }
    ap["model"] = snap.ap_known ? snap.ap_model : 0;

    JsonObject ctrl = doc["controller"].to<JsonObject>();
    ctrl["present"] = snap.controller_present;
    ctrl["forced"] = snap.controller_forced;
    ctrl["override"] = AppConfig::addressToString(snap.controller_override);
    ctrl["age_ms"] = snap.controller_present ? snap.controller_age_ms : 0;
    addAddress(ctrl, "address", snap.controller_present, snap.controller_addr);
    addHexAddress(ctrl, "address_hex", snap.controller_present,
                  snap.controller_addr);

    JsonObject instruments = doc["instruments"].to<JsonObject>();
    const bool heading_mag = ac42_value_fresh(&snap.heading_mag_deg, now, 5000);
    const bool heading_true = ac42_value_fresh(&snap.heading_true_deg, now, 5000);
    addValue(instruments, "heading_mag_deg", snap.heading_mag_deg, now);
    addValue(instruments, "heading_true_deg", snap.heading_true_deg, now);
    if (heading_mag) {
        instruments["heading_deg"] =
            roundf(snap.heading_mag_deg.value * 10.0f) / 10.0f;
        instruments["heading_ref"] = "M";
    } else if (heading_true) {
        instruments["heading_deg"] =
            roundf(snap.heading_true_deg.value * 10.0f) / 10.0f;
        instruments["heading_ref"] = "T";
    } else {
        instruments["heading_deg"] = nullptr;
        instruments["heading_ref"] = nullptr;
    }
    addValue(instruments, "variation_deg", snap.variation_deg, now, 10000);
    addValue(instruments, "awa_deg", snap.awa_deg, now);
    addValue(instruments, "aws_kn", snap.aws_kn, now);
    addValue(instruments, "twa_deg", snap.twa_deg, now);
    addValue(instruments, "tws_kn", snap.tws_kn, now);
    addValue(instruments, "twd_mag_deg", snap.twd_mag_deg, now);
    addValue(instruments, "twd_true_deg", snap.twd_true_deg, now);
    addValue(instruments, "sog_kn", snap.sog_kn, now);
    addValue(instruments, "stw_kn", snap.stw_kn, now);
    addValue(instruments, "cog_mag_deg", snap.cog_mag_deg, now);
    addValue(instruments, "cog_true_deg", snap.cog_true_deg, now);
    if (ac42_value_fresh(&snap.cog_mag_deg, now, 5000)) {
        instruments["cog_deg"] =
            roundf(snap.cog_mag_deg.value * 10.0f) / 10.0f;
        instruments["cog_ref"] = "M";
    } else if (ac42_value_fresh(&snap.cog_true_deg, now, 5000)) {
        instruments["cog_deg"] =
            roundf(snap.cog_true_deg.value * 10.0f) / 10.0f;
        instruments["cog_ref"] = "T";
    } else {
        instruments["cog_deg"] = nullptr;
        instruments["cog_ref"] = nullptr;
    }
    addValue(instruments, "depth_m", snap.depth_m, now);
    addValue(instruments, "rudder_deg", snap.rudder_deg, now);
    addValue(instruments, "commanded_rudder_deg", snap.commanded_rudder_deg, now);

    JsonObject can_obj = doc["can"].to<JsonObject>();
    can_obj["ready"] = can.ready;
    can_obj["state"] = N2kBus::stateName(can.state);
    can_obj["bitrate"] = 250000;
    can_obj["rx_frames"] = can.rx_frames;
    can_obj["tx_frames"] = can.tx_frames;
    can_obj["tx_errors"] = can.tx_errors;
    can_obj["tx_failed"] = can.tx_failed;
    can_obj["bus_errors"] = can.bus_errors;
    can_obj["arb_lost"] = can.arb_lost;
    can_obj["rx_missed"] = can.rx_missed;
    can_obj["tx_error_counter"] = can.tx_error_counter;
    can_obj["rx_error_counter"] = can.rx_error_counter;
    if (can.last_rx_ms != 0) {
        can_obj["last_rx_age_ms"] = (uint32_t)(now - can.last_rx_ms);
    } else {
        can_obj["last_rx_age_ms"] = nullptr;
    }

    JsonObject wifi_obj = doc["wifi"].to<JsonObject>();
    wifi_obj["mode"] = wifi.mode;
    wifi_obj["ap_started"] = wifi.ap_started;
    wifi_obj["ap_ssid"] = wifi.ap_ssid;
    wifi_obj["ap_ip"] = wifi.ap_ip;
    wifi_obj["sta_connected"] = wifi.sta_connected;
    wifi_obj["sta_configured"] = wifi.sta_configured;
    wifi_obj["fallback_ap"] = wifi.fallback_ap;
    wifi_obj["sta_ip"] = wifi.sta_ip;
    wifi_obj["hostname"] = wifi.hostname;
    wifi_obj["rssi"] = wifi.rssi;
    wifi_obj["clients"] = wifi.clients;
    wifi_obj["sta_status"] = wifi.sta_status;

    JsonObject config_obj = doc["config"].to<JsonObject>();
    config_obj["controller_override"] =
        AppConfig::addressToString(snap.controller_override);
    config_obj["ap_override"] = AppConfig::addressToString(snap.ap_override);
    config_obj["rudder_timeout_ms"] =
        config_ != nullptr ? config_->rudder_timeout_ms : 1000;
    config_obj["reboot_pending"] = settings_reboot_pending_;

    JsonArray warnings = doc["warnings"].to<JsonArray>();
    if (!can.ready) {
        warnings.add("Bus CAN non prêt.");
    }
    if (!snap.controller_present) {
        warnings.add("Aucun pupitre/contrôleur B&G actif détecté sur le bus.");
    }
    if (!snap.ap_known && !snap.ap_forced) {
        warnings.add("Adresse de l'AC42 non détectée: cible par défaut 0x02.");
    }
    if (!snap.mode_fresh) {
        warnings.add("État du pilote non reçu (PGN 65340/65305).");
    }
    if (!heading_mag && !heading_true) {
        warnings.add("Cap non reçu (PGN 127250).");
    }
    if (snap.engaged && snap.response_valid == false) {
        warnings.add("Niveau de réponse non lu (PGN 130845).");
    }
    if (snap.nfu_active) {
        warnings.add("Barre directe active: relâchez pour arrêter.");
    }

    String out;
    out.reserve(4096);
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebServerApp::handleSettingsGet()
{
    if (config_ == nullptr) {
        sendError(500, "Configuration non initialisée.");
        return;
    }
    const WifiStatus wifi = wifi_ != nullptr ? wifi_->status() : WifiStatus();

    JsonDocument doc;
    doc["ok"] = true;
    doc["ap_ssid"] = config_->ap_ssid;
    doc["ap_password_set"] = !config_->ap_password.isEmpty();
    doc["sta_ssid"] = config_->sta_ssid;
    doc["sta_password_set"] = !config_->sta_password.isEmpty();
    doc["hostname"] = config_->hostname;
    doc["controller_override"] =
        AppConfig::addressToString(config_->controller_override);
    doc["ap_override"] = AppConfig::addressToString(config_->ap_override);
    doc["rudder_timeout_ms"] = config_->rudder_timeout_ms;
    doc["ap_ip"] = wifi.ap_ip;
    doc["sta_ip"] = wifi.sta_ip;
    doc["wifi_mode"] = wifi.mode;
    doc["sta_connected"] = wifi.sta_connected;
    doc["ap_started"] = wifi.ap_started;
    doc["fallback_ap"] = wifi.fallback_ap;
    doc["mac"] = WiFi.macAddress();
    doc["version"] = AC42_FW_VERSION;
    doc["reboot_pending"] = settings_reboot_pending_;

    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebServerApp::handleSettingsPost()
{
    if (config_ == nullptr) {
        sendError(500, "Configuration non initialisée.");
        return;
    }
    const String content_type = server_.header("Content-Type");
    if (!ensureJson()) {
        if (log_ != nullptr) {
            log_->add("Réglages refusés: Content-Type '%s'", content_type.c_str());
        }
        return;
    }

    JsonDocument doc;
    const String body = requestBody();
    if (log_ != nullptr) {
        log_->add("Réglages: POST reçu (%u octets, type '%s')",
                  (unsigned)body.length(), content_type.c_str());
    }
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        if (log_ != nullptr) {
            log_->add("Réglages refusés: JSON invalide (%s)", err.c_str());
        }
        sendError(400, "JSON invalide.");
        return;
    }

    const AppConfig old = *config_;
    String error;
    auto reject = [&](int code, const String &message) {
        *config_ = old;
        if (log_ != nullptr) {
            log_->add("Réglages refusés: %s", message.c_str());
        }
        sendError(code, message);
    };

    if (doc["ap_ssid"].is<const char *>()) {
        String value = doc["ap_ssid"].as<String>();
        value.trim();
        if (value.isEmpty()) {
            reject(400, "Le nom du réseau Wi-Fi est vide.");
            return;
        }
        config_->ap_ssid = value;
    }
    if (doc["ap_password"].is<const char *>()) {
        const String value = doc["ap_password"].as<String>();
        if (!value.isEmpty()) {
            config_->ap_password = value;
        }
    }
    if (doc["sta_ssid"].is<const char *>()) {
        String value = doc["sta_ssid"].as<String>();
        value.trim();
        config_->sta_ssid = value;
        if (value.isEmpty()) {
            config_->sta_password = "";
        }
    }
    if (doc["sta_password"].is<const char *>()) {
        const String value = doc["sta_password"].as<String>();
        if (!value.isEmpty()) {
            config_->sta_password = value;
        }
    }
    if (doc["hostname"].is<const char *>()) {
        String value = doc["hostname"].as<String>();
        value.trim();
        value.toLowerCase();
        config_->hostname = value;
    }
    if (doc["controller_override"].is<const char *>()) {
        int value = -1;
        if (!AppConfig::parseAddress(doc["controller_override"].as<String>(),
                                     value)) {
            reject(400, "Adresse du contrôleur invalide.");
            return;
        }
        config_->controller_override = value;
    }
    if (doc["ap_override"].is<const char *>()) {
        int value = -1;
        if (!AppConfig::parseAddress(doc["ap_override"].as<String>(), value)) {
            reject(400, "Adresse AC42 invalide.");
            return;
        }
        config_->ap_override = value;
    }
    if (doc["rudder_timeout_ms"].is<int>()) {
        config_->rudder_timeout_ms = doc["rudder_timeout_ms"].as<uint32_t>();
    }
    if (config_->sta_ssid.isEmpty()) {
        config_->sta_password = "";
    }

    if (!config_->isValid(&error)) {
        reject(400, error);
        return;
    }

    const bool network_changed = !config_->networkEquals(old);
    const bool reboot_required = network_changed || settings_reboot_pending_;
    if (!config_->save()) {
        reject(500, "Écriture des réglages impossible (NVS).");
        return;
    }
    if (reboot_required) {
        settings_reboot_pending_ = true;
    }

    JsonDocument out;
    out["ok"] = true;
    out["reboot_required"] = reboot_required;
    out["reboot_pending"] = settings_reboot_pending_;
    out["message"] = reboot_required
                         ? "Réglages enregistrés. Redémarrage nécessaire pour appliquer les paramètres réseau."
                         : "Réglages enregistrés.";
    String payload;
    serializeJson(out, payload);
    sendJson(200, payload);
    if (log_ != nullptr) {
        log_->add("Réglages enregistrés%s",
                  reboot_required ? " (redémarrage requis)" : "");
    }
}

void WebServerApp::handleMode()
{
    if (controller_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }
    if (!ensureJson()) {
        return;
    }
    JsonDocument doc;
    const String body = requestBody();
    if (body.isEmpty() || deserializeJson(doc, body)) {
        sendError(400, "JSON invalide.");
        return;
    }
    const char *mode = doc["mode"].as<const char *>();
    if (mode == nullptr) {
        sendError(400, "Champ 'mode' manquant.");
        return;
    }
    ac42_mode_t target;
    if (strcmp(mode, "standby") == 0) {
        target = AC42_MODE_STANDBY;
    } else if (strcmp(mode, "auto") == 0) {
        target = AC42_MODE_AUTO;
    } else if (strcmp(mode, "wind") == 0) {
        target = AC42_MODE_WIND;
    } else {
        sendError(400, "Mode attendu: standby, auto ou wind.");
        return;
    }
    sendCommandResult(controller_->setMode(target));
}

void WebServerApp::handleCourse()
{
    if (controller_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }
    if (!ensureJson()) {
        return;
    }
    JsonDocument doc;
    const String body = requestBody();
    if (body.isEmpty() || deserializeJson(doc, body)) {
        sendError(400, "JSON invalide.");
        return;
    }
    const char *dir = doc["side"].as<const char *>();
    if (dir == nullptr) {
        dir = doc["dir"].as<const char *>();
    }
    const int degrees = doc["deg"].is<int>() ? doc["deg"].as<int>() : 0;
    if (dir == nullptr || (degrees != 1 && degrees != 10)) {
        sendError(400, "Champs attendus: side=stbd|port, deg=1|10.");
        return;
    }
    bool starboard;
    if (strcmp(dir, "stbd") == 0 || strcmp(dir, "starboard") == 0 ||
        strcmp(dir, "bear") == 0) {
        starboard = true;
    } else if (strcmp(dir, "port") == 0 || strcmp(dir, "luff") == 0) {
        starboard = false;
    } else {
        sendError(400, "Direction attendue: stbd/bear ou port/luff.");
        return;
    }
    sendCommandResult(controller_->changeCourse(starboard, degrees));
}

void WebServerApp::handleTack()
{
    if (controller_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }
    if (!ensureJson()) {
        return;
    }
    JsonDocument doc;
    const String body = requestBody();
    if (body.isEmpty() || deserializeJson(doc, body)) {
        sendError(400, "JSON invalide.");
        return;
    }
    const char *dir = doc["dir"].as<const char *>();
    if (dir == nullptr) {
        sendError(400, "Champ 'dir' manquant.");
        return;
    }
    bool starboard;
    if (strcmp(dir, "stbd") == 0 || strcmp(dir, "starboard") == 0) {
        starboard = true;
    } else if (strcmp(dir, "port") == 0) {
        starboard = false;
    } else {
        sendError(400, "Direction attendue: port ou stbd.");
        return;
    }
    sendCommandResult(controller_->tack(starboard));
}

void WebServerApp::handleRudder()
{
    if (controller_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }
    if (!ensureJson()) {
        return;
    }
    JsonDocument doc;
    const String body = requestBody();
    if (body.isEmpty() || deserializeJson(doc, body)) {
        sendError(400, "JSON invalide.");
        return;
    }
    const char *dir = doc["dir"].as<const char *>();
    if (dir == nullptr) {
        sendError(400, "Champ 'dir' manquant.");
        return;
    }
    ac42_rudder_dir_t target;
    if (strcmp(dir, "stbd") == 0) {
        target = AC42_RUDDER_STBD;
    } else if (strcmp(dir, "port") == 0) {
        target = AC42_RUDDER_PORT;
    } else if (strcmp(dir, "stop") == 0) {
        target = AC42_RUDDER_STOP;
    } else {
        sendError(400, "Direction attendue: port, stbd ou stop.");
        return;
    }
    sendCommandResult(controller_->rudder(target));
}

void WebServerApp::handleResponse()
{
    if (controller_ == nullptr) {
        sendError(500, "Contrôleur non initialisé.");
        return;
    }
    if (!ensureJson()) {
        return;
    }
    JsonDocument doc;
    const String body = requestBody();
    if (body.isEmpty() || deserializeJson(doc, body)) {
        sendError(400, "JSON invalide.");
        return;
    }
    const char *set = doc["set"].as<const char *>();
    if (set == nullptr) {
        sendError(400, "Champ 'set' manquant.");
        return;
    }
    ac42_response_set_t target;
    if (strcmp(set, "auto") == 0) {
        target = AC42_RESPONSE_AUTO;
    } else if (strcmp(set, "low") == 0) {
        target = AC42_RESPONSE_LOW;
    } else if (strcmp(set, "high") == 0) {
        target = AC42_RESPONSE_HIGH;
    } else {
        sendError(400, "Réglage attendu: auto, low ou high.");
        return;
    }
    sendCommandResult(controller_->setResponse(target));
}

void WebServerApp::handleReboot()
{
    JsonDocument doc;
    doc["ok"] = true;
    doc["message"] = "Redémarrage…";
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
    scheduleReboot(500);
    settings_reboot_pending_ = false;
}

void WebServerApp::handleLog()
{
    if (log_ == nullptr) {
        sendJson(200, "[]");
        return;
    }
    sendJson(200, log_->toJson());
}

void WebServerApp::handleNotFound()
{
    if (server_.uri().startsWith("/api/")) {
        sendError(404, "Route inconnue.");
    } else {
        server_.send(404, "text/plain; charset=utf-8", "Page inconnue");
    }
}

void WebServerApp::scheduleReboot(uint32_t delay_ms)
{
    reboot_pending_ = true;
    reboot_at_ms_ = millis() + delay_ms;
}

