#include "wifi_manager.h"

#include "app_util.h"

#include <ESPmDNS.h>
#include <WiFi.h>

namespace {
constexpr uint32_t kStaConnectTimeoutMs = 15000;
constexpr uint32_t kStaDisconnectApDelayMs = 10000;
constexpr uint32_t kStaReconnectIntervalMs = 30000;
constexpr uint32_t kApRetryIntervalMs = 5000;
constexpr uint32_t kMdnsRetryIntervalMs = 10000;
} // namespace

bool WifiManager::startAp()
{
    if (ap_started_) {
        return true;
    }
    const uint32_t now = app_now_ms();
    if (last_ap_attempt_ms_ != 0 &&
        (uint32_t)(now - last_ap_attempt_ms_) < kApRetryIntervalMs) {
        return false;
    }
    last_ap_attempt_ms_ = now;

    if (config_ == nullptr) {
        return false;
    }

    ap_started_ = WiFi.softAP(config_->ap_ssid.c_str(),
                              config_->ap_password.c_str(),
                              6, 0, 4);
    if (ap_started_) {
        const String ip = WiFi.softAPIP().toString();
        if (log_ != nullptr) {
            log_->add("Wi-Fi AP: IP %s - http://%s/", ip.c_str(), ip.c_str());
        }
        last_ip_log_ms_ = now;
        startMdns();
    } else if (log_ != nullptr) {
        log_->add("Wi-Fi: démarrage du point d'accès impossible");
    }
    return ap_started_;
}

void WifiManager::startMdns()
{
    if (mdns_started_ || config_ == nullptr) {
        return;
    }
    const uint32_t now = app_now_ms();
    if (last_mdns_attempt_ms_ != 0 &&
        (uint32_t)(now - last_mdns_attempt_ms_) < kMdnsRetryIntervalMs) {
        return;
    }
    last_mdns_attempt_ms_ = now;

    if (MDNS.begin(config_->hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        mdns_started_ = true;
        if (log_ != nullptr) {
            log_->add("mDNS: http://%s.local", config_->hostname.c_str());
        }
    } else if (log_ != nullptr) {
        log_->add("mDNS: démarrage impossible (IP directe disponible)");
    }
}

bool WifiManager::begin(const AppConfig &config, AppLog *log)
{
    config_ = &config;
    log_ = log;

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(config.hostname.c_str());

    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));

    sta_configured_ = !config.sta_ssid.isEmpty();
    if (sta_configured_) {
        WiFi.setAutoReconnect(true);
        WiFi.begin(config.sta_ssid.c_str(), config.sta_password.c_str());
        sta_attempt_ms_ = app_now_ms();
        if (log_ != nullptr) {
            log_->add("Wi-Fi: connexion STA à '%s' (AP de secours après %lu s)",
                      config.sta_ssid.c_str(),
                      (unsigned long)(kStaConnectTimeoutMs / 1000));
        }
    } else {
        startAp();
    }

    return ap_started_;
}

void WifiManager::loop()
{
    if (config_ == nullptr) {
        return;
    }

    const uint32_t now = app_now_ms();
    const bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
        if (!last_sta_connected_) {
            last_sta_connected_ = true;
            ever_sta_connected_ = true;
            const String ip = WiFi.localIP().toString();
            if (log_ != nullptr) {
                log_->add("Wi-Fi STA: IP %s - http://%s/ (RSSI %d dBm)",
                          ip.c_str(), ip.c_str(), WiFi.RSSI());
            }
            last_ip_log_ms_ = now;
            startMdns();
        }
    } else {
        if (last_sta_connected_) {
            last_sta_connected_ = false;
            last_disconnect_ms_ = now;
            if (log_ != nullptr) {
                log_->add("Wi-Fi STA déconnecté");
            }
        }

        if (sta_configured_) {
            if (!ap_started_) {
                const bool never_connected = !ever_sta_connected_;
                const uint32_t since = never_connected
                                           ? sta_attempt_ms_
                                           : last_disconnect_ms_;
                const uint32_t timeout = never_connected
                                             ? kStaConnectTimeoutMs
                                             : kStaDisconnectApDelayMs;
                if ((uint32_t)(now - since) >= timeout) {
                    if (log_ != nullptr) {
                        log_->add("Wi-Fi: réseau '%s' inaccessible, activation de l'AP de secours",
                                  config_->sta_ssid.c_str());
                    }
                    startAp();
                }
            }

            if ((uint32_t)(now - last_reconnect_ms_) >
                kStaReconnectIntervalMs) {
                last_reconnect_ms_ = now;
                WiFi.reconnect();
            }
        } else if (!ap_started_) {
            startAp();
        }
    }

    if (!mdns_started_ && (ap_started_ || connected)) {
        startMdns();
    }

    if ((uint32_t)(now - last_ip_log_ms_) >= 60000u) {
        last_ip_log_ms_ = now;
        if (connected) {
            const String ip = WiFi.localIP().toString();
            if (log_ != nullptr) {
                log_->add("Wi-Fi: IP STA %s", ip.c_str());
            }
        }
        if (ap_started_) {
            const String ip = WiFi.softAPIP().toString();
            if (log_ != nullptr) {
                log_->add("Wi-Fi: IP AP %s", ip.c_str());
            }
        }
    }
}

WifiStatus WifiManager::status() const
{
    WifiStatus s;
    s.ap_started = ap_started_;
    s.sta_configured = sta_configured_;
    s.sta_connected = (WiFi.status() == WL_CONNECTED);
    s.fallback_ap = ap_started_ && !s.sta_connected && sta_configured_;
    s.ap_ssid = config_ != nullptr ? config_->ap_ssid : String();
    s.ap_ip = ap_started_ ? WiFi.softAPIP().toString() : String();
    s.sta_ip = s.sta_connected ? WiFi.localIP().toString() : String();
    s.hostname = config_ != nullptr ? config_->hostname : String();
    if (ap_started_) {
        s.mode = s.sta_connected ? "AP+STA"
                                 : (sta_configured_ ? "AP de secours" : "AP");
    } else if (s.sta_connected) {
        s.mode = "STA";
    } else {
        s.mode = sta_configured_ ? "connexion STA" : "AP";
    }
    s.rssi = s.sta_connected ? WiFi.RSSI() : 0;
    s.clients = ap_started_ ? WiFi.softAPgetStationNum() : 0;
    s.sta_status = (int)WiFi.status();
    return s;
}
