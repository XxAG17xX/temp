#pragma once

#include "protocols.h"
#include "utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace reed_solomon {

/// @brief Returns the Reed-Solomon parity bytes of a given packet
/// @param data the packet to encode
/// @param rscode the Reed-Solomon code parameters
/// @return the parity bytes
std::vector<uint8_t> compute_parity(const std::vector<uint8_t> &data,
                                    const RSCode &rscode);

/// @brief Corrects errors in a whole packet using Reed-Solomon error
/// correction. If possible, the string is returned without errors or parity
/// bytes. If not possible, an empty optional is returned.
/// @param data the packet to correct
/// @param rscode the Reed-Solomon code parameters
/// @return the corrected data (if possible)
std::optional<std::vector<uint8_t>>
decode_packet(const std::vector<uint8_t> &data, const RSCode &rscode);

/// @brief Encodes a packet using Reed-Solomon error correction
/// @tparam T the type of struct to encode
/// @param data struct to encode
/// @param rscode the Reed-Solomon code parameters
/// @return the pkt packet
template <typename T>
std::vector<uint8_t> encode_packet(const T &data, const RSCode &rscode) {
  // Unpack the parameters
  auto [n, k] = rscode;

  if (k > n || k == 0) {
    throw std::runtime_error("Invalid block size\n 0 <= k <= n\n");
  }

  // Turn the struct into a byte vector
  std::vector<uint8_t> bytes = util::struct_to_bytes(data);

  // Reserve space for the packet data
  std::vector<uint8_t> pkt;
  pkt.reserve(bytes.size() +                     // Original data size
              (bytes.size() / k) * (n - k) +     // Per-block parity bytes
              (bytes.size() % k ? (n - k) : 0)); // Last block parity bytes

  // Iterate over data blocks
  for (size_t offset = 0; offset < bytes.size(); offset += k) {
    // Calculate the size of the current block (may be smaller for the last
    // block)
    size_t block_size = std::min(static_cast<size_t>(k), bytes.size() - offset);

    // Create a temporary vector for the current block
    std::vector<uint8_t> block(bytes.begin() + offset,
                               bytes.begin() + offset + block_size);

    // Pad the last block if needed
    if (block.size() < k) {
      block.resize(k, 0);
    }

    // Compute parity for this block
    std::vector<uint8_t> parity = compute_parity(block, rscode);

    // Append block data and parity to the packet
    pkt.insert(pkt.end(), block.begin(), block.end());
    pkt.insert(pkt.end(), parity.begin(), parity.end());
  }

  return pkt;
}
} // namespace reed_solomon