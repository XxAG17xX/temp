#pragma once
#include "health/health.h"
#include "protocols.h"
#include "terrain_gen/terrain_gen.h"

#include <asio.hpp>

// This should be the same among all rover instances
constexpr double rock_chance = 0.2;
constexpr int seed = 8675309;

using asio::ip::udp;

/// @brief Abstraction of Simulated Moon Rover
class Rover {
private:
  // Sends a message to the Earth Base, includes error-handling
  void send_message(const std::vector<uint8_t> &message, udp::socket &socket,
                    const udp::endpoint &endpoint);

  // Blocks main thread until Earth base ACKs discovery
  void wait_for_discovery_response();

  // Waits for movement command from earth base, then preforms movement
  void wait_for_movement();

  // Sends a reply to the movement
  void send_movement_response(bool status, bool moved);

  // Waits for terrain command from earth base, then sends back terrain
  void wait_for_terrain();

  // Monitor Health Stats
  void monitor_health();

  // sockets used for different interactions
  udp::socket m_discovery_socket, m_movement_socket, m_terrain_socket,
      m_status_socket;

  // Discovered by earth base
  std::atomic<bool> m_discovered = false;

  // address of earth base
  asio::ip::address m_earthbase_addr;

  // Reed-Solomon error correction level
  // Atomic to allow for thread-safe updates during negotiation
  std::atomic<uint8_t> m_rscode_level;

  // ID for this rover instance given by earth base
  uint8_t m_id;

  // Local position of rover on terrain
  int m_x, m_y;

  // Sequence number for movement command
  bool m_movement_seq_num;

  // Instance of Terrain Generation class
  TerrainGenerator m_tgen = TerrainGenerator(rock_chance, seed);

  // Threads for Rover/Base interaction
  std::thread discovery_thread, movement_thread, terrain_thread, health_thread;

public:
  /// @brief Default constructor for Rover Class
  /// @param io_context Socket context for the rover
  /// @param server_ip IP address of the Earth Base
  Rover(asio::io_context &io_context, const std::string &server_ip);

  /// @brief Starts the rover's network interactions
  void start();

  /// @brief Calls the TerrainGenerator to print the current terrain
  void printCurrentTerrain();
};