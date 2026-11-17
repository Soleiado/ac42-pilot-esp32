#pragma once

#include <Arduino.h>

/*
 * Persistent settings stored in NVS.  Passwords are never returned by the web
 * API; the UI only learns whether one is configured.
 */
struct AppConfig {
    String   ap_ssid;
    String   ap_password;
    String   sta_ssid;
    String   sta_password;
    String   hostname;
    int      controller_override = -1; /* -1 = auto, otherwise 0..255 */
    int      ap_override = -1;         /* -1 = auto, otherwise 0..255 */
    uint32_t rudder_timeout_ms = 1000;

    void begin();
    bool save() const;
    void resetDefaults();

    bool isValid(String *error = nullptr) const;
    bool networkEquals(const AppConfig &other) const;

    static String defaultApSsid();
    static String addressToString(int value);
    static bool   parseAddress(const String &text, int &out);
};
