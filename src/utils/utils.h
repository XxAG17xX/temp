#pragma once
#include "protocols.h"

#include <any>
#include <asio/detail/socket_ops.hpp>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace util {
// NOTE: Once reed-solomon is done, the checksum functions will be moved to
// error_correction.h

/// @brief Computes the Internet Checksum of a piece of data
/// @param data data to compute checksum of
/// @return 16-bit internet checksum
uint16_t computeInternetChecksum(const std::string &data);

/// @brief Takes a packet and verifies the internet checksum
/// @param packet Packet of data to verify
/// @return boolean of whether the checksum is correct
bool validInternetChecksum(const std::string &packet);

/// @brief Function Template that takes a struct, appends the checksum, and
/// returns the result as a string
/// @tparam T The type of struct (see examples in protocols.h)
/// @param req The struct instance
/// @return the packet with appended checksum
template <typename T> std::string construct_packet(const T &req) {
  std::string pkt;
  pkt.resize(sizeof(T) + sizeof(uint16_t));
  std::memcpy(&pkt[0], reinterpret_cast<const char *>(&req), sizeof(T));

  std::string struct_only = pkt.substr(0, sizeof(T));
  uint16_t checksum = computeInternetChecksum(struct_only);

  // Convert to network endianness
  uint16_t network_chksum =
      asio::detail::socket_ops::host_to_network_short(checksum);

  std::memcpy(&pkt[sizeof(T)], &network_chksum, sizeof(network_chksum));

  return pkt;
}

/// @brief Converts a struct to a vector of bytes
/// @tparam T Struct Type
/// @param data Struct to convert
/// @return Byte vector of struct
template <typename T> std::vector<uint8_t> struct_to_bytes(const T &data) {
  // String Case
  if (typeid(T) == typeid(std::string)) {
    std::string str = std::any_cast<std::string>(data);
    return std::vector<uint8_t>(str.begin(), str.end());
  }

  std::vector<uint8_t> bytes(sizeof(T));
  const uint8_t *d_ptr = reinterpret_cast<const uint8_t *>(&data);

  for (size_t i = 0; i < sizeof(T); ++i) {
    bytes[i] = d_ptr[i];
  }

  return bytes;
}

/// @brief Gets current time of computer
/// @return current time in 64-bit epoch time
uint64_t current_time();

} // namespace util
