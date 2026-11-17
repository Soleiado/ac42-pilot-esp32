#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class AppLog {
public:
    void begin();
    void add(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void add(const String &message);
    String toJson() const;
    void clear();

private:
    static constexpr size_t kCapacity = 48;
    static constexpr size_t kMessageSize = 140;

    struct Entry {
        uint32_t ms;
        char message[kMessageSize];
    };

    Entry entries_[kCapacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    SemaphoreHandle_t mutex_ = nullptr;
};
