#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "ac42_controller.h"
#include "app_config.h"
#include "app_log.h"
#include "n2k_bus.h"
#include "wifi_manager.h"

class WebServerApp {
public:
    void begin(Ac42Controller *controller, WifiManager *wifi,
               AppConfig *config, AppLog *log, N2kBus *bus);
    void loop();

private:
    void handleRoot();
    void handleFavicon();
    void handleStatus();
    void handleSettingsGet();
    void handleSettingsPost();
    void handleMode();
    void handleCourse();
    void handleTack();
    void handleRudder();
    void handleResponse();
    void handleReboot();
    void handleLog();
    void handleNotFound();

    bool ensureJson();
    String requestBody();
    void sendJson(int code, const String &payload);
    void sendError(int code, const String &message);
    void sendCommandResult(const Ac42CommandResult &result);
    void scheduleReboot(uint32_t delay_ms);

    WebServer server_{80};
    Ac42Controller *controller_ = nullptr;
    WifiManager *wifi_ = nullptr;
    AppConfig *config_ = nullptr;
    AppLog *log_ = nullptr;
    N2kBus *bus_ = nullptr;

    bool reboot_pending_ = false;
    uint32_t reboot_at_ms_ = 0;
    bool settings_reboot_pending_ = false;
};
