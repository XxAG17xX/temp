#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

/// @brief The maximum allowed packet size (1024)
constexpr int MAX_PACKET_SIZE = 2 << 9;

/// @brief HELO Handshake
const char HELO[4] = {'H', 'E', 'L', 'O'};

/// @brief Acknowledgement chars
const char ACK[3] = {'A', 'C', 'K'};
/// @brief Negative acknowledgement chars
const char NAK[3] = {'N', 'A', 'K'};

/// @brief Ports used for each interaction
enum PORTS {
  DISCOVERY = 2263,
  MOVEMENT_CMD,  // Send Rover commands over this port
  MOVEMENT_RESP, // Send Rover responses on this port
  TERRAIN,
  LOCATION,
  STATUS
};

/// @brief Rover Direction
enum DIRECTION { UP = 0, DOWN, LEFT, RIGHT };

/// @brief Request Fields for Discovery Interaction. Consists of "HELO"
struct DiscoveryRequest {
  char helo[4];
  uint64_t timestamp;

  DiscoveryRequest() { std::memcpy(helo, HELO, sizeof(helo)); }
};

/// @brief Response Fields for Discovery Interaction. Consists of "HELO",
/// ACK/NAK, then the rover's designated ID
struct DiscoveryResponse {
  char helo[4];
  char status[3];
  uint8_t rover_id = 0;
  uint64_t timestamp;

  // I hate this, please somebody find a better way
  DiscoveryResponse() {
    std::memcpy(helo, HELO, sizeof(helo));
    std::memcpy(status, ACK, sizeof(status));
  }
};

/// @brief Reed-Solomon Code Parameters (n, k)
/// @details n = number of symbols in a block
/// @details k = number of symbols in a block that are data
/// @details n - k = number of symbols in a block that are parity (must be
/// positive)
struct RSCode {
  uint8_t n; // Number of symbols in a block
  uint8_t k; // Number of symbols in a block that are data

  // Constexpr constructor
  constexpr RSCode(uint8_t n = 255, uint8_t k = 223)
      : n((n > k && k > 0) ? n
                           : throw "Invalid parameters for Reed-Solomon code"),
        k(k) {}
};

/// @brief Reed-Solomon Code Levels used in error-correction negotiation
/// @details The levels are defined as n, k pairs, where each level gives twice
/// the parity of the previous level. Max 8 levels
constexpr auto RS_LEVELS = []() {
  std::array<RSCode, 8> levels{}; // Max 8 levels within 255 limit
  uint8_t k = 223;
  uint8_t p = 1;

  for (size_t index = 0; k + p <= 255 && index < levels.size(); ++index) {
    uint8_t n = k + p;
    levels[index] = RSCode{n, k};
    p *= 2;
  }
  return levels;
}();

/// @brief The maximum number of retries for a packet
constexpr int MAX_RETRIES = 5;

/// @brief The maximum timeout for a packet
constexpr int MAX_TIMEOUT_MS = 3000;

/// @brief Request Fields for Movement Interaction.
/// Consists of Rover ID, Direction (see DIRECTION), timestamp in 64-bit epoch
/// time, and sequence number
struct MoveRequest {
  uint32_t rover_id;
  DIRECTION direction;
  uint64_t timestamp;
  bool sequence_num;
};

/// @brief Response Fields for Movement Interaction.
struct MoveResponse {
  uint32_t rover_id;
  char status[3]; // Was the checksum correct?
  bool moved;     // Could the rover move?
  bool sequence_num;
  // Coordinates
  int x;
  int y;
  uint64_t timestamp;

  MoveResponse() { std::memcpy(status, ACK, sizeof(status)); }
};

struct StatusRequest {
  uint32_t rover_id;
  uint64_t timestamp;
};

struct StatusResponse {
  uint32_t rover_id;
  char status[3]; // ACK/NAK
  float battery_level;
  float temperature;
  bool emergency;
  char message[64];
  uint64_t timestamp;

  StatusResponse() {
    std::memcpy(status, ACK, sizeof(status));
    std::memset(message, 0, sizeof(message));
  }
};