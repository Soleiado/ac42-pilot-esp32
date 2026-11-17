#include "n2k_bus.h"

#include "app_util.h"
#include "board_pins.h"

#include <driver/twai.h>
#include <string.h>

bool N2kBus::installDriver()
{
    if (installed_) {
        return true;
    }

    last_install_attempt_ms_ = app_now_ms();

    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)BOARD_CAN_TX_PIN,
                                    (gpio_num_t)BOARD_CAN_RX_PIN,
                                    TWAI_MODE_NORMAL);
    general.rx_queue_len = 64;
    general.tx_queue_len = 16;

    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_250KBITS();
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&general, &timing, &filter);
    if (err != ESP_OK) {
        if (log_ != nullptr) {
            log_->add("CAN: installation impossible (%s)", esp_err_to_name(err));
        }
        return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
        twai_driver_uninstall();
        if (log_ != nullptr) {
            log_->add("CAN: démarrage impossible (%s)", esp_err_to_name(err));
        }
        return false;
    }

    installed_ = true;
    ready_ = true;
    if (log_ != nullptr) {
        log_->add("CAN prêt: 250 kbit/s, TX=GPIO%d RX=GPIO%d",
                  BOARD_CAN_TX_PIN, BOARD_CAN_RX_PIN);
    }
    return true;
}

bool N2kBus::begin(AppLog *log)
{
    log_ = log;
    const bool ok = installDriver();
    if (task_ == nullptr) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            taskEntry, "n2k_rx", 4096, this, 12, &task_, 1);
        if (created != pdPASS) {
            task_ = nullptr;
            if (log_ != nullptr) {
                log_->add("CAN: tâche de réception non créée");
            }
        }
    }
    return ok;
}

void N2kBus::setFrameHandler(FrameHandler handler, void *context)
{
    handler_ = handler;
    handler_context_ = context;
}

void N2kBus::taskEntry(void *arg)
{
    static_cast<N2kBus *>(arg)->taskLoop();
}

void N2kBus::taskLoop()
{
    for (;;) {
        if (!installed_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        twai_message_t message = {};
        const esp_err_t err = twai_receive(&message, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            if (!message.extd || message.rtr || message.data_length_code == 0) {
                continue;
            }
            n2k_frame_t frame = {};
            frame.id = message.identifier;
            frame.dlc = message.data_length_code > 8 ? 8 : message.data_length_code;
            memcpy(frame.data, message.data, frame.dlc);

            portENTER_CRITICAL(&stats_mux_);
            stats_.rx_frames++;
            stats_.last_rx_ms = app_now_ms();
            portEXIT_CRITICAL(&stats_mux_);

            if (handler_ != nullptr) {
                handler_(frame, handler_context_);
            }
        } else if (err != ESP_ERR_TIMEOUT) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

bool N2kBus::send(const n2k_frame_t &frame)
{
    if (!installed_ || !ready_) {
        portENTER_CRITICAL(&stats_mux_);
        stats_.tx_errors++;
        portEXIT_CRITICAL(&stats_mux_);
        return false;
    }

    twai_message_t message = {};
    message.identifier = frame.id;
    message.extd = 1;
    message.rtr = 0;
    message.data_length_code = frame.dlc > 8 ? 8 : frame.dlc;
    memcpy(message.data, frame.data, message.data_length_code);

    const esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(20));
    portENTER_CRITICAL(&stats_mux_);
    if (err == ESP_OK) {
        stats_.tx_frames++;
    } else {
        stats_.tx_errors++;
    }
    portEXIT_CRITICAL(&stats_mux_);
    return err == ESP_OK;
}

void N2kBus::updateStats(CanStats &out)
{
    twai_status_info_t info = {};
    if (installed_ && twai_get_status_info(&info) == ESP_OK) {
        portENTER_CRITICAL(&stats_mux_);
        stats_.state = info.state;
        stats_.tx_error_counter = info.tx_error_counter;
        stats_.rx_error_counter = info.rx_error_counter;
        stats_.tx_failed = info.tx_failed_count;
        stats_.rx_missed = info.rx_missed_count;
        stats_.arb_lost = info.arb_lost_count;
        stats_.bus_errors = info.bus_error_count;
        stats_.ready = (info.state == TWAI_STATE_RUNNING);
        portEXIT_CRITICAL(&stats_mux_);
    }

    portENTER_CRITICAL(&stats_mux_);
    out = stats_;
    portEXIT_CRITICAL(&stats_mux_);
    out.ready = ready_ && out.ready;
}

CanStats N2kBus::stats()
{
    CanStats copy = {};
    updateStats(copy);
    return copy;
}

void N2kBus::loop()
{
    if (!installed_) {
        if ((uint32_t)(app_now_ms() - last_install_attempt_ms_) > 3000u) {
            installDriver();
        }
        return;
    }

    twai_status_info_t info = {};
    if (twai_get_status_info(&info) != ESP_OK) {
        return;
    }

    if (info.state == TWAI_STATE_BUS_OFF) {
        ready_ = false;
        if ((uint32_t)(app_now_ms() - last_recovery_ms_) > 1000u) {
            last_recovery_ms_ = app_now_ms();
            if (log_ != nullptr) {
                log_->add("CAN: bus-off, récupération");
            }
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(100));
            if (twai_start() == ESP_OK) {
                ready_ = true;
                if (log_ != nullptr) {
                    log_->add("CAN: bus rétabli");
                }
            }
        }
    } else if (info.state == TWAI_STATE_STOPPED) {
        ready_ = false;
        if (twai_start() == ESP_OK) {
            ready_ = true;
            if (log_ != nullptr) {
                log_->add("CAN: redémarré");
            }
        }
    } else {
        ready_ = (info.state == TWAI_STATE_RUNNING);
    }
}

const char *N2kBus::stateName(uint32_t state)
{
    switch (state) {
    case TWAI_STATE_STOPPED:    return "stopped";
    case TWAI_STATE_RUNNING:    return "running";
    case TWAI_STATE_BUS_OFF:    return "bus_off";
    case TWAI_STATE_RECOVERING: return "recovering";
    default:                    return "unknown";
    }
}
