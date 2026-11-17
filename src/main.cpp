#include <Arduino.h>
#include <esp_system.h>

#include "ac42_controller.h"
#include "app_config.h"
#include "app_log.h"
#include "app_util.h"
#include "n2k_bus.h"
#include "version.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace {
AppConfig config;
AppLog logs;
WifiManager wifi;
N2kBus bus;
Ac42Controller pilot;
WebServerApp web;

void onCanFrame(const n2k_frame_t &frame, void *context)
{
    auto *controller = static_cast<Ac42Controller *>(context);
    if (controller != nullptr) {
        controller->onFrame(frame, app_now_ms());
    }
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(200);
#if ARDUINO_USB_CDC_ON_BOOT
    /* Do not block command handling when no USB host is connected. */
    Serial.setTxTimeoutMs(0);
#endif

    logs.begin();
    logs.add("==============================================");
    logs.add("%s %s", AC42_FW_NAME, AC42_FW_VERSION);
    logs.add("Carte Waveshare ESP32-S3-RS485-CAN");
    logs.add("Reset: %d", (int)esp_reset_reason());

    config.begin();
    logs.add("Config: AP='%s' hôte='%s' contrôleur=%s AC42=%s barre=%lums",
             config.ap_ssid.c_str(), config.hostname.c_str(),
             AppConfig::addressToString(config.controller_override).c_str(),
             AppConfig::addressToString(config.ap_override).c_str(),
             (unsigned long)config.rudder_timeout_ms);

    wifi.begin(config, &logs);

    pilot.begin(&bus, &config, &logs);
    bus.setFrameHandler(onCanFrame, &pilot);
    bus.begin(&logs);

    web.begin(&pilot, &wifi, &config, &logs, &bus);

    const WifiStatus status = wifi.status();
    if (status.sta_connected) {
        logs.add("Wi-Fi STA: IP %s - http://%s/",
                 status.sta_ip.c_str(), status.sta_ip.c_str());
    }
    if (status.ap_started) {
        logs.add("Wi-Fi AP: IP %s - http://%s/ (%s)",
                 status.ap_ip.c_str(), status.ap_ip.c_str(),
                 status.mode.c_str());
    }
    if (!status.sta_connected && !status.ap_started) {
        if (!config.sta_ssid.isEmpty()) {
            logs.add("Wi-Fi: connexion STA en cours; AP de secours si échec");
        } else {
            logs.add("Wi-Fi: AP en cours de démarrage");
        }
    }
}

void loop()
{
    wifi.loop();
    pilot.tick();
    bus.loop();
    web.loop();
    delay(1);
}
