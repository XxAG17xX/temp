#include "rover.h"
#include "error_correction/error_correction.h"
#include "health/health.h"
#include "utils.h"

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#endif

Rover::Rover(asio::io_context &io_context, const std::string &server_ip)
    : m_discovery_socket(io_context), m_movement_socket(io_context),
      m_terrain_socket(io_context), m_status_socket(io_context),
      m_earthbase_addr(asio::ip::address::from_string(server_ip)), m_id(99),
      m_x(0), m_y(0), m_movement_seq_num(1) {
// Windows-specific: Disable connection reset behavior
#ifdef _WIN32
  BOOL bNewBehavior = FALSE;
  DWORD dwBytesReturned = 0;
  WSAIoctl(m_discovery_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
  WSAIoctl(m_movement_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
  WSAIoctl(m_terrain_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
  WSAIoctl(m_health_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif
  // Open Discovery Socket
  m_discovery_socket.open(udp::v4());

  // Open Movement Socket
  m_movement_socket.open(udp::v4());
  m_movement_socket.bind(udp::endpoint(udp::v4(), PORTS::MOVEMENT_CMD));

  // Open Terrain Socket
  m_terrain_socket.open(udp::v4());

  // Open Status Socket
  m_status_socket.open(udp::v4());
  m_status_socket.bind(udp::endpoint(udp::v4(), PORTS::STATUS));
}

void Rover::start() {
  // Create the discovery mutex/cv
  std::mutex discovery_mutex;
  std::condition_variable discovery_cv;

  // Start discovery process
  std::thread discovery_thread([this, &discovery_cv]() {
    this->wait_for_discovery_response();
    // Notify main thread when discovery completes
    discovery_cv.notify_one();
  });

  udp::endpoint discovery_endpoint(m_earthbase_addr, PORTS::DISCOVERY);

  // Construct discovery request packet
  DiscoveryRequest d_req = {};
  d_req.timestamp = util::current_time();
  auto pkt = reed_solomon::encode_packet(d_req, RS_LEVELS[m_rscode_level]);

  // Keep sending discovery requests until discovered or timeout
  while (!m_discovered) {
    // Send discovery request
    pkt = reed_solomon::encode_packet(d_req, RS_LEVELS[m_rscode_level]);
    send_message(pkt, m_discovery_socket, discovery_endpoint);

    // Wait for up to 3 seconds or until notified that discovery is complete
    std::unique_lock<std::mutex> lock(discovery_mutex);
    discovery_cv.wait_for(lock, std::chrono::milliseconds(MAX_TIMEOUT_MS),
                          [this] { return m_discovered.load(); });

    // If discovered during the wait, exit the loop
    // (This stops us having to wait for the full 3 seconds if we get a
    // response)
    if (m_discovered)
      break;
  }

  // Join the discovery thread
  discovery_thread.join();

  std::cout << "Discovery complete. Rover ID: " << static_cast<int>(m_id)
            << std::endl;

  // Start threads for movement and terrain commands
  std::thread movement_thread(&Rover::wait_for_movement, this);
  std::thread terrain_thread(&Rover::wait_for_terrain, this);
  std::thread health_thread(&Rover::monitor_health, this);
  movement_thread.detach();
  terrain_thread.detach();
  health_thread.detach();
}

void Rover::send_message(const std::vector<uint8_t> &message,
                         udp::socket &socket, const udp::endpoint &endpoint) {
  // Check if the socket is open
  if (!socket.is_open()) {
    std::cerr << "Error: Socket is not open!" << std::endl;
    return;
  }
  socket.send_to(asio::buffer(message), endpoint);
}

void Rover::wait_for_terrain() {
  char data[MAX_PACKET_SIZE];
  udp::endpoint sender_endpoint;

  try {
    std::cout << "Waiting for terrain data...\n" << std::endl;

    if (!m_terrain_socket.is_open()) {
      std::cerr << "Error: Terrain socket is not open!\n" << std::endl;
    }

    m_terrain_socket.receive_from(asio::buffer(data, sizeof(data)),
                                  sender_endpoint);
    std::cout << "Received terrain data: " << data << std::endl;
  } catch (const std::system_error &e) {
    std::cerr << "Error receiving terrain data: " << e.what()
              << " (Error Code: " << e.code().value() << ")" << std::endl;
  }

  // TODO: Implement Terrain Interaction
}

void Rover::wait_for_movement() {
  char data[MAX_PACKET_SIZE];
  udp::endpoint sender_endpoint;
  std::cout << "Rover listening for movement commands on port: "
            << m_movement_socket.local_endpoint().port() << std::endl;

  // This is running on its own thread, so no need to worry about blocking
  while (1) {
    std::memset(data, 0, sizeof(data));

    // Wait to receive data from earth base
    size_t length = m_movement_socket.receive_from(
        asio::buffer(data, sizeof(data)), sender_endpoint);

    auto packet = reed_solomon::decode_packet(
        std::vector<uint8_t>(data, data + length), RS_LEVELS[m_rscode_level]);

    // If not decoded successfully
    if (!packet) {
      send_movement_response(false, false);
      continue;
    }

    // Process the movement command
    MoveRequest req;
    std::memcpy(&req, packet->data(), sizeof(MoveRequest));

    std::cout << "\nReceived movement command: Rover ID = " << req.rover_id
              << ", Direction = " << req.direction
              << ", Sequence = " << (req.sequence_num ? "1" : "0") << std::endl;

    // If this is a duplicate, send response but don't execute movement again
    if (req.sequence_num == m_movement_seq_num) {
      send_movement_response(true, false);
      continue;
    }

    // Update sequence number
    m_movement_seq_num = req.sequence_num;

    // Update rover's position based on the direction
    // (or don't if there's a rock)
    bool moved = true;
    auto terrain = m_tgen.getTerrain(m_x, m_y);
    switch (req.direction) {
    case DIRECTION::UP:
      if (terrain[1][2])
        moved = false;
      else
        m_y--;
      break;
    case DIRECTION::DOWN:
      if (terrain[3][2])
        moved = false;
      else
        m_y++;
      break;
    case DIRECTION::LEFT:
      if (terrain[2][1])
        moved = false;
      else
        m_x--;
      break;
    case DIRECTION::RIGHT:
      if (terrain[2][3])
        moved = false;
      else
        m_x++;
      break;
    }

    // Print terrain on rover-side
    if (!moved) {
      std::cout << "Rock detected! Staying in current position\n";
    }
    printCurrentTerrain();

    // Send an ACK response
    send_movement_response(true, moved);
  }
}

void Rover::send_movement_response(bool status, bool moved) {
  // Construct response
  MoveResponse resp;
  resp.rover_id = m_id;
  strncpy(resp.status, status ? ACK : NAK, 3);
  resp.moved = moved;
  resp.sequence_num = m_movement_seq_num;
  resp.x = m_x;
  resp.y = m_y;
  resp.timestamp = util::current_time();

  // Encode Packet with Reed-Solomon level
  auto pkt = reed_solomon::encode_packet(resp, RS_LEVELS[m_rscode_level]);

  // Resolve endpoint
  auto endpoint = udp::endpoint(m_earthbase_addr, PORTS::MOVEMENT_RESP);

  // Send the message
  send_message(pkt, m_movement_socket, endpoint);
}

void Rover::wait_for_discovery_response() {
  char data[MAX_PACKET_SIZE];
  udp::endpoint sender_endpoint;

  // While this rover hasn't been discovered yet
  while (!m_discovered.load()) {
    if (!m_discovery_socket.is_open()) {
      std::cerr << "Socket is not open!" << std::endl;
      return;
    }

    std::memset(data, 0, MAX_PACKET_SIZE);

    // Block this thread until a packet is received
    m_discovery_socket.non_blocking(false);

    // Wait for a packet from the Earth base
    size_t length = m_discovery_socket.receive_from(
        asio::buffer(data, MAX_PACKET_SIZE), sender_endpoint);

    std::cout << "Received " << length << " bytes from "
              << sender_endpoint.address().to_string() << ":"
              << sender_endpoint.port() << std::endl;

    auto packet = reed_solomon::decode_packet(
        std::vector<uint8_t>(data, data + length), RS_LEVELS[m_rscode_level]);

    // Check if this is a valid response with a valid checksum
    if (packet.has_value()) {
      // Process discovery response
      DiscoveryResponse resp;
      std::memcpy(&resp, packet->data(), sizeof(DiscoveryResponse));

      std::cout << "Received discovery response with status: " << resp.status
                << std::endl;

      // Check if it's an ACK
      if (strncmp(resp.status, ACK, 3) == 0) {
        m_id = resp.rover_id;
        m_discovered.store(true);
        break;
      } else {
        std::cout << "Received NAK response, will increase RS level."
                  << std::endl;

        // Increase RS level (max 7)
        m_rscode_level = m_rscode_level != 7 ? m_rscode_level + 1 : 7;
      }
    } else {
      std::cout << "Received invalid checksum in discovery response, will "
                   "increase RS level."
                << std::endl;
      m_rscode_level = m_rscode_level != 7 ? m_rscode_level + 1 : 7;
    }
  }
}

void Rover::printCurrentTerrain() {
  std::cout << "\nCoordinates: (" << m_x << ", " << m_y << ")\n";
  m_tgen.printTerrain(m_x, m_y);
}

void Rover::monitor_health() {
  udp::endpoint earth_endpoint(
      m_earthbase_addr,
      PORTS::STATUS); // Earth listening for unsolicited alerts

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5)); // check every 5s

    HealthData health = HealthData::get_current_health();
    if (health.emergency) {
      StatusResponse resp;
      resp.rover_id = m_id;
      std::memcpy(resp.status, ACK, sizeof(resp.status));
      resp.battery_level = health.battery_level;
      resp.temperature = health.temperature;
      resp.emergency = true;
      std::strncpy(resp.message, health.message.c_str(),
                   sizeof(resp.message) - 1);
      resp.timestamp = util::current_time();

      auto pkt = reed_solomon::encode_packet(resp, RS_LEVELS[m_rscode_level]);
      send_message(pkt, m_status_socket, earth_endpoint);

      std::cout << "🚨 Sent emergency alert to Earth: " << health.message
                << "\n";
    }
  }
}
