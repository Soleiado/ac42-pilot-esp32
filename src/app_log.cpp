#include "app_log.h"

#include "app_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void AppLog::begin()
{
    if (mutex_ == nullptr) {
        mutex_ = xSemaphoreCreateMutex();
    }
}

void AppLog::add(const char *fmt, ...)
{
    char buffer[kMessageSize];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    add(String(buffer));
}

void AppLog::add(const String &message)
{
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, pdMS_TO_TICKS(50));
    }

    char serial_message[kMessageSize];
    uint32_t stamp = 0;
    {
        Entry &entry = entries_[head_];
        entry.ms = app_now_ms();
        strncpy(entry.message, message.c_str(), sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';
        stamp = entry.ms;
        memcpy(serial_message, entry.message, sizeof(serial_message));
        head_ = (head_ + 1) % kCapacity;
        if (count_ < kCapacity) {
            count_++;
        }
    }

    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }

    Serial.printf("[%8lu] %s\n", (unsigned long)(stamp / 1000), serial_message);
}

static void appendEscaped(String &out, const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        const unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04X", c);
                out += buf;
            } else {
                out += (char)c;
            }
            break;
        }
    }
}

String AppLog::toJson() const
{
    String out;
    out.reserve(4096);
    out += "[";

    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, pdMS_TO_TICKS(50));
    }

    const size_t start = (count_ < kCapacity) ? 0 : head_;
    for (size_t i = 0; i < count_; i++) {
        const Entry &entry = entries_[(start + i) % kCapacity];
        if (i > 0) {
            out += ",";
        }
        out += "{\"ms\":";
        out += String(entry.ms);
        out += ",\"t\":\"";
        appendEscaped(out, entry.message);
        out += "\"}";
    }

    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }

    out += "]";
    return out;
}

void AppLog::clear()
{
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, pdMS_TO_TICKS(50));
    }
    head_ = 0;
    count_ = 0;
    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }
}
