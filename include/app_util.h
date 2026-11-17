#pragma once

#include <Arduino.h>
#include <esp_timer.h>

static inline uint32_t app_now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline float app_rad_to_deg(float rad)
{
    return rad * 57.29577951308232f;
}

static inline float app_mps_to_kn(float mps)
{
    return mps * 1.9438444924406046f;
}
