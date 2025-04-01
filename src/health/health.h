// === health.h ===
#pragma once
#include <cstdint>
#include <string>
#include <asio.hpp>


/// @brief Struct to hold rover health data
struct HealthData {
    float battery_level;    // 0 - 100%
    float temperature;      // Celsius
    float motor_load;       // 0 - 100%
    int signal_strength;    // 0 to 5 bars
    uint64_t system_uptime; // ms since boot
    int error_code;         // 0 = OK, 1+ = issue
    bool emergency;         // any critical issue?
    std::string message;    // human-readable problem

    static HealthData get_current_health();
    void print() const;
};

void listen_for_health_requests(asio::io_context& io, unsigned short port);
