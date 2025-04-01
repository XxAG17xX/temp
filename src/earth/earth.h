#pragma once
#include "protocols.h"

#include <asio.hpp>
#include <optional>
#include <vector>

using asio::ip::udp;

/// @brief Type alias for a rover endpoint
struct RoverEndpoint {
  udp::endpoint endpoint; // The UDP endpoint of the rover
  uint8_t rs_level;       // The Reed-Solomon error level for this rover
  bool hasACKed; // Whether the earthbase has acknowledged the discovery request
  bool movement_seq_num; // Sequence number for movement command
};

/// @brief Abstraction of Simulated Earth base
class EarthBase {
private:
  // Sockets for discovery and movement
  udp::socket m_discovery_socket, m_movement_socket;

  // List of active rovers
  std::vector<std::optional<RoverEndpoint>> m_active_rovers;

  // Thread for listening for rover discovery
  std::thread m_listener_thread;

  // Get the address of the rover at the given index
  RoverEndpoint *get_rover_endpoint_by_idx(const unsigned int idx);

  // Get the rover endpoint by its UDP endpoint
  RoverEndpoint *get_rover_by_endpoint(const udp::endpoint &endpoint);

  // Get the iterator to the rover endpoint by its UDP endpoint
  std::vector<std::optional<RoverEndpoint>>::iterator
  get_rover_itr(const udp::endpoint &endpoint);

  // Blocking function to listen for rover discovery
  // This runs on its own thread
  void listen_for_rovers();

  // Send a message to a rover endpoint
  void send_message(const std::vector<uint8_t> &message, udp::socket &socket,
                    udp::endpoint endpoint);

public:
  /// @brief Default constructor for EarthBase class
  /// @param io_context Socket context for the Earth base
  EarthBase(asio::io_context &io_context);

  /// @brief Sends a command to a given rover to move up/down/left/right
  /// @param rover_idx ID of the rover to send command to
  /// @param direction Direction to move the rover
  /// @return error code
  size_t send_movment_command(uint32_t rover_idx, DIRECTION direction);

  /// @brief Starts the Earth base networking interactions
  void start();
  void request_health_report(uint32_t rover_idx);
};