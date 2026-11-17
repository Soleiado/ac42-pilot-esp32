#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

#include "app_log.h"
#include "n2k_codec.h"

struct CanStats {
    bool     ready;
    uint32_t state;
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t tx_errors;
    uint32_t bus_errors;
    uint32_t arb_lost;
    uint32_t rx_missed;
    uint32_t tx_failed;
    uint8_t  tx_error_counter;
    uint8_t  rx_error_counter;
    uint32_t last_rx_ms;
};

class N2kBus {
public:
    using FrameHandler = void (*)(const n2k_frame_t &frame, void *context);

    bool begin(AppLog *log);
    void loop();
    bool send(const n2k_frame_t &frame);
    CanStats stats();
    bool ready() const { return ready_; }
    void setFrameHandler(FrameHandler handler, void *context);

    static const char *stateName(uint32_t state);

private:
    static void taskEntry(void *arg);
    void taskLoop();
    bool installDriver();
    void updateStats(CanStats &out);

    AppLog *log_ = nullptr;
    FrameHandler handler_ = nullptr;
    void *handler_context_ = nullptr;
    TaskHandle_t task_ = nullptr;
    volatile bool installed_ = false;
    volatile bool ready_ = false;
    uint32_t last_install_attempt_ms_ = 0;
    uint32_t last_recovery_ms_ = 0;

    CanStats stats_ = {};
    portMUX_TYPE stats_mux_ = portMUX_INITIALIZER_UNLOCKED;
};
