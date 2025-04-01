
// === health.cpp ===
#include "health.h"
#include "protocols.h"
#include "utils.h"
#include <asio.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using asio::ip::udp;

/// Get real battery level from host system
float get_battery_level() {
#ifdef _WIN32
  SYSTEM_POWER_STATUS status;
  if (GetSystemPowerStatus(&status)) {
    return static_cast<float>(status.BatteryLifePercent);
  }
#endif
  return -1.0f; // error or unsupported
}

/// Get system uptime in milliseconds (like rover mission clock)
uint64_t get_uptime_ms() {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

HealthData HealthData::get_current_health() {
  HealthData data;

  // === Real telemetry ===
  data.battery_level = get_battery_level();
  data.system_uptime = get_uptime_ms();

  // === Simulated telemetry ===
  data.temperature = 15.0f + static_cast<float>(std::rand() % 30); // 15–44°C
  data.motor_load = static_cast<float>(std::rand() % 101);         // 0–100%
  data.signal_strength = std::rand() % 6;                          // 0–5 bars

  // === Emergency condition check ===
  data.error_code = 0;
  data.emergency = false;

  if (data.battery_level >= 0 && data.battery_level < 20.0f) {
    data.error_code = 1;
    data.message = " CRITICAL: Rover battery is dangerously low";
    data.emergency = true;
  } else if (data.temperature > 40.0f) {
    data.error_code = 2;
    data.message = " WARNING: Internal temperature exceeding threshold";
    data.emergency = true;
  } else if (data.motor_load > 90.0f) {
    data.error_code = 3;
    data.message = " Motor stress high — possible terrain hazard";
    data.emergency = true;
  } else if (data.signal_strength < 1) {
    data.error_code = 4;
    data.message = " Weak signal: potential comms blackout";
    data.emergency = true;
  } else {
    data.message = " All systems nominal — rover healthy";
  }

  return data;
}

void HealthData::print() const {
  std::cout << "----------------------------------\n";
  std::cout << "  ROVER SYSTEM HEALTH REPORT\n";
  std::cout << "----------------------------------\n";
  std::cout << "Battery Level    : " << battery_level << "%\n";
  std::cout << "Temperature      : " << temperature << " °C\n";
  std::cout << "Motor Load       : " << motor_load << " %\n";
  std::cout << "Signal Strength  : " << signal_strength << " bars\n";
  std::cout << "System Uptime    : " << system_uptime << " ms\n";
  std::cout << "Emergency Status : " << (emergency ? " YES" : "OK") << "\n";
  std::cout << "Message          : " << message << "\n";
  std::cout << "----------------------------------\n";
}

void listen_for_health_requests(asio::io_context& io, unsigned short port) {
  udp::socket socket(io, udp::endpoint(udp::v4(), port));
  std::cout << "Listening for health requests on port " << port << "...\n";

  char data[MAX_PACKET_SIZE];
  udp::endpoint sender;

  while (true) {
      std::memset(data, 0, sizeof(data));
      size_t len = socket.receive_from(asio::buffer(data), sender);

      if (!util::validInternetChecksum(std::string(data, len))) {
          std::cout << "Invalid checksum. Ignoring packet.\n";
          continue;
      }

      StatusRequest req;
      std::memcpy(&req, data, sizeof(StatusRequest));

      HealthData health = HealthData::get_current_health();

      StatusResponse resp;
      resp.rover_id = req.rover_id;
      std::memcpy(resp.status, ACK, sizeof(resp.status));
      resp.battery_level = health.battery_level;
      resp.temperature = health.temperature;
      resp.emergency = health.emergency;
      std::strncpy(resp.message, health.message.c_str(), sizeof(resp.message) - 1);
      resp.timestamp = util::current_time();

      std::string pkt = util::construct_packet(resp);
      socket.send_to(asio::buffer(pkt), sender);
  }
}
