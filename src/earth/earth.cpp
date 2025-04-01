#include "earth.h"
#include "error_correction/error_correction.h"
#include "utils.h"

#include <asio/ts/buffer.hpp>   //memory movement
#include <asio/ts/internet.hpp> //internet
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#endif

EarthBase::EarthBase(asio::io_context &io_context)
    : m_discovery_socket(io_context,
                         udp::endpoint(udp::v4(), PORTS::DISCOVERY)),
      m_movement_socket(io_context,
                        udp::endpoint(udp::v4(), PORTS::MOVEMENT_RESP)),
      m_active_rovers() {
#ifdef _WIN32
  BOOL bNewBehavior = FALSE;
  DWORD dwBytesReturned = 0;
  WSAIoctl(m_discovery_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
  WSAIoctl(m_movement_socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior,
           sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

  m_discovery_socket.set_option(asio::socket_base::reuse_address(true));
  m_movement_socket.set_option(asio::socket_base::reuse_address(true));

  std::cout << "Earth base listening on port " << PORTS::DISCOVERY << "..."
            << std::endl;
}

void EarthBase::listen_for_rovers() {
  char data[MAX_PACKET_SIZE];
  // This runs on its own thread so no need to worry about blocking
  while (true) {
    // Wait for a message
    asio::ip::udp::endpoint sender_endpoint;
    size_t length = m_discovery_socket.receive_from(
        asio::buffer(data, sizeof(data)), sender_endpoint);

    std::cout << "\nReceived from Rover: " << data << std::endl;

    // Check if the endpoint is new
    // RoverEndpoint is a pair of udp::endpoint and uint8_t
    if (get_rover_by_endpoint(sender_endpoint)) {
      auto existing_rover = get_rover_by_endpoint(sender_endpoint);
      // If we already ACKed this rover and it's still sending
      // discovery packets, we can assume it has a higher RS level
      if (existing_rover->hasACKed && existing_rover->rs_level != 7) {
        existing_rover->rs_level++;
      }
    } else {
      // Add the endpoint to the list of active rovers
      m_active_rovers.push_back(RoverEndpoint{sender_endpoint, 0, false, 1});
    }

    // Get the rover endpoint index
    auto rover_endpoint = get_rover_by_endpoint(sender_endpoint);
    if (!rover_endpoint) {
      std::cerr << "Error: Failed to find rover endpoint that should exist"
                << std::endl;
      continue; // Skip this iteration and wait for next packet
    }

    // Decode the packet
    std::optional<std::vector<uint8_t>> req_packet =
        reed_solomon::decode_packet(std::vector<uint8_t>(data, data + length),
                                    RS_LEVELS[rover_endpoint->rs_level]);

    // Fill the response packet
    DiscoveryResponse d_resp{};
    strncpy(d_resp.status, req_packet.has_value() ? ACK : NAK, 3);
    d_resp.rover_id = get_rover_itr(sender_endpoint) - m_active_rovers.begin();
    d_resp.timestamp = util::current_time();

    // Encode the response packet with the current RS level for this rover
    auto resp_packet = reed_solomon::encode_packet(
        d_resp, RS_LEVELS[rover_endpoint->rs_level]);

    m_discovery_socket.send_to(asio::buffer(resp_packet), sender_endpoint);

    // If the packet was too erroneous to decode, we need to increment the RS
    // level
    if (!req_packet.has_value()) {
      if (rover_endpoint->rs_level != 7) {
        rover_endpoint->rs_level++;
      }
    } else {
      rover_endpoint->hasACKed = true;
    }
  }
}

void EarthBase::send_message(const std::vector<uint8_t> &message,
                             udp::socket &socket, udp::endpoint endpoint) {
  try {
    udp::endpoint local_endpoint(endpoint.address().is_v4() ? udp::v4()
                                                            : udp::v6(),
                                 socket.local_endpoint().port());

    socket.send_to(asio::buffer(message), endpoint);
  } catch (const std::exception &e) {
    std::cerr << "\nError sending message: " << e.what() << std::endl;
  }
}

void EarthBase::start() {
  m_listener_thread = std::thread(&EarthBase::listen_for_rovers, this);
}

std::vector<std::optional<RoverEndpoint>>::iterator
EarthBase::get_rover_itr(const udp::endpoint &endpoint) {
  return std::find_if(m_active_rovers.begin(), m_active_rovers.end(),
                      [&endpoint](const std::optional<RoverEndpoint> &rover) {
                        return rover.has_value() && rover->endpoint == endpoint;
                      });
}

RoverEndpoint *EarthBase::get_rover_by_endpoint(const udp::endpoint &endpoint) {
  auto rover_it = get_rover_itr(endpoint);
  if (rover_it != m_active_rovers.end()) {
    // Return a reference to the value inside the optional
    return &rover_it->value();
  }
  return nullptr;
}

RoverEndpoint *EarthBase::get_rover_endpoint_by_idx(const unsigned int idx) {

  // If out of bounds
  if (idx < 0 || idx >= (unsigned int)m_active_rovers.size()) {
    return nullptr;
  }

  if (!m_active_rovers[idx]) {
    return nullptr;
  }

  // Return otherwise
  return &(m_active_rovers[idx].value());
}

size_t EarthBase::send_movment_command(uint32_t rover_idx,
                                       DIRECTION direction) {
  // Get the rover's discovery endpoint and change the port to ROVER_MOVEMENT
  auto rover_endpoint = get_rover_endpoint_by_idx(rover_idx);
  if (!rover_endpoint) {
    std::cerr << "Rover not found at index " << rover_idx << std::endl;
    return 1;
  }

  MoveRequest req = {rover_idx, direction, util::current_time(),
                     !rover_endpoint->movement_seq_num};

  // Update the sequence number
  rover_endpoint->movement_seq_num = req.sequence_num;

  // Copy endpoint and update the rover endpoint to the movement port
  auto rover_movement_endpoint = *rover_endpoint;
  rover_movement_endpoint.endpoint.port(PORTS::MOVEMENT_CMD);

  // Encode the request packet with the current RS level for this rover
  auto request_packet =
      reed_solomon::encode_packet(req, RS_LEVELS[rover_endpoint->rs_level]);

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    std::cout << "Sending movement command (attempt " << (attempt + 1) << "/"
              << MAX_RETRIES << ") to "
              << rover_movement_endpoint.endpoint.address().to_string() << ":"
              << rover_movement_endpoint.endpoint.port() << std::endl;

    // Send move command
    send_message(request_packet, m_movement_socket,
                 rover_movement_endpoint.endpoint);

    // Wait for response
    char data[MAX_PACKET_SIZE];
    udp::endpoint sender_endpoint;

    // Set to non-blocking mode so we can handle timeouts
    m_movement_socket.non_blocking(true);

    auto start_time = std::chrono::steady_clock::now();
    size_t length = 0;
    bool received = false;

    // Wait for a response or timeout
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
               .count() < MAX_TIMEOUT_MS) {
      try {
        length = m_movement_socket.receive_from(
            asio::buffer(data, sizeof(data)), sender_endpoint);
        received = true;
        break;
      } catch (const asio::system_error &e) {
        // If no data is available, we can ignore the error
        if (e.code() == asio::error::would_block) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          continue;
        } else {
          // Some other error
          std::cerr << "Error receiving movement response: " << e.what()
                    << std::endl;
          break;
        }
      }
    }

    // Set socket back to blocking mode
    m_movement_socket.non_blocking(false);

    if (!received) {
      std::cout << "Timeout waiting for response, retrying..." << std::endl;
      continue; // Try again
    }

    std::cout << "Received " << length << " bytes from "
              << sender_endpoint.address().to_string() << ":"
              << sender_endpoint.port() << std::endl;

    // Create string with exact length
    std::optional<std::vector<uint8_t>> resp_packet =
        reed_solomon::decode_packet(std::vector<uint8_t>(data, data + length),
                                    RS_LEVELS[rover_endpoint->rs_level]);

    // Check if packet could be decoded
    if (!resp_packet) {
      std::cout << "Could not decode movement response, retrying..."
                << std::endl;
      continue;
    }

    // Process the movement response
    MoveResponse resp;
    std::memcpy(&resp, resp_packet->data(), sizeof(MoveResponse));

    std::cout << "Movement response:\n\tRover ID = " << resp.rover_id
              << ",\n\tStatus = " << resp.status
              << ",\n\tMoved = " << (resp.moved ? "true" : "false")
              << ",\n\tPosition = (" << resp.x << "," << resp.y << ")"
              << std::endl;

    // Successfully received and decoded response
    return 0;
  }

  // All attempts failed
  std::cout << "Failed to get valid movement response after " << MAX_RETRIES
            << " attempts" << std::endl;
  return 1;
}
void EarthBase::request_health_report(uint32_t rover_idx)
{
  auto rover_endpoint = get_rover_endpoint_by_idx(rover_idx);
  if (!rover_endpoint)
  {
    std::cerr << "Rover not found at index " << rover_idx << std::endl;
    return;
  }

  // Set health check port
  udp::endpoint health_endpoint = rover_endpoint->endpoint;
  health_endpoint.port(PORTS::STATUS);  // This is usually port 2268

  StatusRequest req;
  req.rover_id = rover_idx;
  req.timestamp = util::current_time();

  // Encode the request with RS level
  auto request_packet = reed_solomon::encode_packet(req, RS_LEVELS[rover_endpoint->rs_level]);

  std::cout << "Requesting health report from Rover " << rover_idx << "...\n";

  // Send the packet
  send_message(request_packet, m_movement_socket, health_endpoint);

  char data[MAX_PACKET_SIZE];
  udp::endpoint sender_endpoint;

  // Set to non-blocking to handle timeout
  m_movement_socket.non_blocking(true);

  auto start_time = std::chrono::steady_clock::now();
  size_t length = 0;
  bool received = false;

  while (std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start_time)
             .count() < MAX_TIMEOUT_MS)
  {
    try
    {
      length = m_movement_socket.receive_from(
          asio::buffer(data, sizeof(data)), sender_endpoint);
      received = true;
      break;
    }
    catch (const asio::system_error &e)
    {
      if (e.code() == asio::error::would_block)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
      else
      {
        std::cerr << "Error receiving health response: " << e.what() << "\n";
        break;
      }
    }
  }

  m_movement_socket.non_blocking(false);

  if (!received)
  {
    std::cout << "Health report request timed out.\n";
    return;
  }

  auto decoded_packet = reed_solomon::decode_packet(std::vector<uint8_t>(data, data + length),
                                                    RS_LEVELS[rover_endpoint->rs_level]);

  if (!decoded_packet)
  {
    std::cout << "Could not decode health response.\n";
    return;
  }

  StatusResponse resp;
  std::memcpy(&resp, decoded_packet->data(), sizeof(StatusResponse));

  std::cout << "\n ROVER HEALTH REPORT:\n";
  std::cout << "Battery     : " << resp.battery_level << "%\n";
  std::cout << "Temperature : " << resp.temperature << " C\n";
  std::cout << "Emergency   : " << (resp.emergency ? "YES" : "NO") << "\n";
  std::cout << "Message     : " << resp.message << "\n";
  std::cout << "Timestamp   : " << resp.timestamp << "\n";
}
