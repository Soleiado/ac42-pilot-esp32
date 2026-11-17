#pragma once

#include <Arduino.h>

#include "app_config.h"
#include "app_log.h"

struct WifiStatus {
    bool     ap_started = false;
    bool     sta_connected = false;
    bool     fallback_ap = false;
    bool     sta_configured = false;
    String   ap_ssid;
    String   ap_ip;
    String   sta_ip;
    String   hostname;
    String   mode;
    int      rssi = 0;
    uint8_t  clients = 0;
    int      sta_status = 0;
};

class WifiManager {
public:
    bool begin(const AppConfig &config, AppLog *log);
    void loop();
    WifiStatus status() const;

private:
    bool startAp();
    void startMdns();

    const AppConfig *config_ = nullptr;
    AppLog *log_ = nullptr;
    bool ap_started_ = false;
    bool sta_configured_ = false;
    bool ever_sta_connected_ = false;
    bool last_sta_connected_ = false;
    bool mdns_started_ = false;
    uint32_t sta_attempt_ms_ = 0;
    uint32_t last_disconnect_ms_ = 0;
    uint32_t last_reconnect_ms_ = 0;
    uint32_t last_ap_attempt_ms_ = 0;
    uint32_t last_mdns_attempt_ms_ = 0;
    uint32_t last_ip_log_ms_ = 0;
};
