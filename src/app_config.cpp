#include "app_config.h"

#include <Preferences.h>
#include <stdio.h>
#include <stdlib.h>

namespace {
constexpr const char *kNamespace = "ac42cfg";
constexpr const char *kDefaultApPassword = "ac42pilot";
constexpr uint32_t kConfigVersion = 2;
}

String AppConfig::defaultApSsid()
{
    char buf[32];
    const uint64_t mac = ESP.getEfuseMac();
    snprintf(buf, sizeof(buf), "AC42-Pilot-%04X", (unsigned)(mac & 0xFFFFu));
    return String(buf);
}

void AppConfig::resetDefaults()
{
    ap_ssid = defaultApSsid();
    ap_password = kDefaultApPassword;
    sta_ssid = "";
    sta_password = "";
    hostname = "ac42-pilot";
    controller_override = -1;
    ap_override = -1;
    rudder_timeout_ms = 1000;
}

void AppConfig::begin()
{
    resetDefaults();

    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        save();
        return;
    }

    const uint32_t config_version = prefs.getUInt("cfg_ver", 0);
    ap_ssid = prefs.getString("ap_ssid", ap_ssid);
    ap_password = prefs.getString("ap_pass", ap_password);
    sta_ssid = prefs.getString("sta_ssid", "");
    sta_password = prefs.getString("sta_pass", "");
    hostname = prefs.getString("hostname", hostname);
    controller_override = prefs.getInt("ctrl_ovr", -1);
    ap_override = prefs.getInt("ap_ovr", -1);
    rudder_timeout_ms = prefs.getUInt("rudder_ms", 1000);
    prefs.end();

    bool dirty = false;
    if (config_version < kConfigVersion) {
        /* Earlier experimental firmware stored a preconfigured STA network.
         * Clear it on upgrade: no local Wi-Fi credentials are kept by default.
         * The user can configure a network from the settings page. */
        sta_ssid = "";
        sta_password = "";
        dirty = true;
    }

    String error;
    if (!isValid(&error)) {
        /* Corrupt or old NVS: fall back to safe defaults rather than starting
         * an access point with an invalid password. */
        resetDefaults();
        dirty = true;
    }
    if (dirty) {
        save();
    }
}

bool AppConfig::save() const
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return false;
    }
    prefs.putString("ap_ssid", ap_ssid);
    prefs.putString("ap_pass", ap_password);
    prefs.putString("sta_ssid", sta_ssid);
    prefs.putString("sta_pass", sta_password);
    prefs.putString("hostname", hostname);
    prefs.putInt("ctrl_ovr", controller_override);
    prefs.putInt("ap_ovr", ap_override);
    prefs.putUInt("rudder_ms", rudder_timeout_ms);
    prefs.putUInt("cfg_ver", kConfigVersion);
    prefs.end();
    return true;
}

bool AppConfig::isValid(String *error) const
{
    auto fail = [error](const char *message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (ap_ssid.length() < 1 || ap_ssid.length() > 31) {
        return fail("Le nom du réseau Wi-Fi doit faire 1 à 31 caractères.");
    }
    if (ap_password.length() < 8 || ap_password.length() > 63) {
        return fail("Le mot de passe Wi-Fi doit faire 8 à 63 caractères.");
    }
    if (sta_ssid.length() > 31) {
        return fail("Le SSID du réseau domestique est trop long.");
    }
    if (!sta_ssid.isEmpty() &&
        (sta_password.length() < 8 || sta_password.length() > 63)) {
        return fail("Le mot de passe du réseau domestique doit faire 8 à 63 caractères.");
    }
    if (hostname.length() < 1 || hostname.length() > 31) {
        return fail("Le nom d'hôte doit faire 1 à 31 caractères.");
    }
    for (size_t i = 0; i < hostname.length(); i++) {
        const char c = hostname[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return fail("Le nom d'hôte ne peut contenir que a-z, 0-9 et '-'.");
        }
    }
    if (controller_override < -1 || controller_override > 253) {
        return fail("Adresse du contrôleur invalide (0x00..0xFD ou auto).");
    }
    if (ap_override < -1 || ap_override > 253) {
        return fail("Adresse AC42 invalide (0x00..0xFD ou auto).");
    }
    if (controller_override >= 0 && ap_override >= 0 &&
        controller_override == ap_override) {
        return fail("Les adresses du contrôleur et de l'AC42 doivent être différentes.");
    }
    if (rudder_timeout_ms < 300 || rudder_timeout_ms > 5000) {
        return fail("Le délai de sécurité barre doit être entre 300 et 5000 ms.");
    }
    return true;
}

bool AppConfig::networkEquals(const AppConfig &other) const
{
    return ap_ssid == other.ap_ssid &&
           ap_password == other.ap_password &&
           sta_ssid == other.sta_ssid &&
           sta_password == other.sta_password &&
           hostname == other.hostname;
}

String AppConfig::addressToString(int value)
{
    if (value < 0) {
        return "auto";
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "0x%02X", value & 0xFF);
    return String(buf);
}

bool AppConfig::parseAddress(const String &text, int &out)
{
    String t = text;
    t.trim();
    t.toLowerCase();
    if (t.isEmpty() || t == "auto" || t == "-1") {
        out = -1;
        return true;
    }
    if (t.startsWith("0x")) {
        t.remove(0, 2);
    }
    if (t.isEmpty() || t.length() > 2) {
        return false;
    }
    char *end = nullptr;
    const long value = strtol(t.c_str(), &end, 16);
    if (end == nullptr || *end != '\0' || value < 0 || value > 253) {
        return false;
    }
    out = (int)value;
    return true;
}
