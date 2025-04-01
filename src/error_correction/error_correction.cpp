#include "error_correction.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace reed_solomon {
// Irreducible polynomial for GF(2^8):
// x^8 + x^4 + x^3 + x^2 + 1
constexpr uint16_t POLYNOMIAL = 0x011D;

// This function generates the exponential table at compile-time
// for the Galois Field (2^8) such that x[i] = 2^i mod POLYNOMIAL
constexpr std::array<uint8_t, 256> generate_exp_table() {
  // Initialize the table
  std::array<uint8_t, 256> table = {};

  uint16_t x = 1;
  for (int i = 0; i < 255; i++) {
    // Store the current value in the table
    table[i] = x;
    // Multiply by 2
    x = x << 1;

    // If the 9th bit is set (x>255),
    // then we need to XOR with the polynomial
    if (x & 0b100000000) {
      x = x ^ POLYNOMIAL;
    }
  }

  // Set last value to the first value
  table[255] = table[0];

  return table;
}

// This function uses the exponential table to generate a log table (also in
// compile-time) Since exp_table[i] <-> exp(i)mod255, then it stands that
// log_table[exp_table[i]] = i <-> log(exp_table[i])mod255
constexpr std::array<uint8_t, 256>
generate_log_table(const std::array<uint8_t, 256> &exp_table) {
  // Initialize the table
  std::array<uint8_t, 256> table = {};

  // log(0) = -infinity but this isn't python
  table[0] = 0;

  // The log table is the inverse of the
  // exponential table, so we can use the exponential table to generate the log
  // table
  for (int i = 0; i < 255; i++) {
    uint8_t idx = exp_table[i];
    table[idx] = i;
  }

  return table;
}

constexpr auto EXPONENTIAL_TABLE = generate_exp_table();
constexpr auto LOGARITHM_TABLE = generate_log_table(EXPONENTIAL_TABLE);

// Addition in a Galois Field is XOR
uint8_t add(const uint8_t a, const uint8_t b) { return a ^ b; }

uint8_t multiply(const uint8_t a, const uint8_t b) {
  // a * 0 = 0 * b = 0
  if (a == 0 || b == 0) {
    return 0;
  }

  // 2^(log2(a)+log2(b))=ab
  // (mod 255 because Galois Field)
  return EXPONENTIAL_TABLE[(LOGARITHM_TABLE[a] + LOGARITHM_TABLE[b]) % 255];
}

uint8_t divide(const uint8_t a, const uint8_t b) {
  // 0/n = 0
  if (a == 0) {
    return 0;
  }

  // n/0 = error
  if (b == 0) {
    throw std::runtime_error("Attempting to divide by 0\n");
  }

  // 2^(log2(a)-log2(b))=a/b
  // The + 255 is to avoid a-b going negative, and is an
  // identity operation in GF(256) regardless
  return EXPONENTIAL_TABLE[(255 + LOGARITHM_TABLE[a] - LOGARITHM_TABLE[b]) %
                           255];
}

// NOTE: A lot of this implementation draws inspiration from this project
// https://github.com/sigh/reed-solomon

// Add coeffs of polynomial together using GF(256) operations
std::vector<uint8_t> add_polynomials(const std::vector<uint8_t> &p1,
                                     const std::vector<uint8_t> &p2) {
  size_t result_size = std::max(p1.size(), p2.size());
  std::vector<uint8_t> poly_result(result_size, 0);

  std::copy(p1.begin(), p1.end(),
            poly_result.begin() + (result_size - p1.size()));

  // Add functions together
  for (size_t i = 0, j = result_size - p2.size(); i < p2.size(); ++i, ++j) {
    poly_result[j] = add(poly_result[j], p2[i]);
  }

  return poly_result;
}

// Find p(x) in Galois Field
uint8_t evaluate_polynomial(const std::vector<uint8_t> &p, uint8_t x) {
  uint8_t y = p[0];
  for (size_t i = 1; i < p.size(); i++) {
    y = add(p[i], multiply(y, x));
  }
  return y;
}

// Multiply two polynomials in Galois Field
std::vector<uint8_t> multiply_polynomials(const std::vector<uint8_t> &p1,
                                          const std::vector<uint8_t> &p2) {
  std::vector<uint8_t> result(p1.size() + p2.size() - 1, 0);

  for (size_t j = 0; j < p2.size(); ++j) {
    for (size_t i = 0; i < p1.size(); ++i) {
      result[i + j] = add(result[i + j], multiply(p1[i], p2[j]));
    }
  }

  return result;
}

// Scale a polynomial by a certain factor in Galois Field
std::vector<uint8_t> scale_polynomial(const std::vector<uint8_t> &p,
                                      const uint8_t scale_factor) {
  std::vector<uint8_t> poly_result = p;
  for (size_t i = 0; i < p.size(); ++i) {
    poly_result[i] = multiply(poly_result[i], scale_factor);
  }
  return poly_result;
}

// Polynomial multiplication at specific index (basically convolution)
uint8_t polynomial_convolution(const std::vector<uint8_t> &p1,
                               const std::vector<uint8_t> &p2, size_t a) {
  uint8_t conv = 0;
  for (size_t i = 0; i <= a; i++) {
    if (i < p1.size() && (a - i) < p2.size()) {
      // sum(p1[i] * p2[a-i])
      conv = add(conv, multiply(p1[p1.size() - i - 1], p2[a - i]));
    }
  }
  return conv;
}

std::vector<uint8_t> compute_parity(const std::vector<uint8_t> &data,
                                    const RSCode &rscode) {
  auto &[n, k] = rscode;
  // Check for invalid block size
  if (k > n) {
    throw std::runtime_error("Invalid block size\n k <= n\n");
  }

  // Calculate the number of parity bits needed, and initialize them to 0
  uint8_t parity_size = n - k;
  std::vector<uint8_t> parity_bits(parity_size, 0);

  // Initialize the generator polynomial (used to extrapolate parity bits)
  std::vector<uint8_t> generator(parity_size + 1, 0);
  generator[0] = 1;

  // Generate the generator polynomial:
  // g(x) = (x - a^1)(x - a^2)...(x - a^(n-k))
  for (uint8_t i = 0; i < parity_size; ++i) {
    int alpha_i = EXPONENTIAL_TABLE[i + 1];
    for (uint8_t j = i + 1; j > 0; --j) {
      generator[j] = add(generator[j], multiply(generator[j - 1], alpha_i));
    }
  }

  // Calculate the parity bits using polynomial division
  uint8_t feedback;
  for (int i = 0; i < k; i++) {
    feedback = add(data[i], parity_bits[0]);

    // Shift the parity bits left
    for (int j = 0; j < parity_size - 1; j++) {
      parity_bits[j] =
          add(parity_bits[j + 1], multiply(feedback, generator[j + 1]));
    }

    // Compute the last parity bit
    parity_bits[parity_size - 1] = multiply(feedback, generator[parity_size]);
  }

  return parity_bits;
}

std::optional<std::vector<uint8_t>>
decode_block(const std::vector<uint8_t> &data, const RSCode &rscode) {
  auto &[n, k] = rscode;

  // Check for invalid block size
  if (k > n) {
    throw std::runtime_error("Invalid block size\nk <= n\n");
  }

  // Check for invalid packet size
  uint8_t parity_size = n - k;
  if (data.size() == 0 || data.size() < n) {
    std::cerr << "Invalid packet size\n";
    return std::nullopt;
  }

  std::vector<uint8_t> data_copy(data.begin(), data.begin() + k);

  // Initialize the syndrome vector
  std::vector<uint8_t> syndromes(parity_size, 0);

  // Calculate the syndromes
  // (fancy name for "error detector numbers")
  for (int i = 0; i < parity_size; ++i) {
    uint8_t syndrome = 0;
    for (int j = 0; j < n; ++j) {
      // Calculate the exponent for the current symbol
      uint8_t exp_idx = ((i + 1) * (n - 1 - j)) % 255;
      syndrome = add(syndrome, multiply(data[j], EXPONENTIAL_TABLE[exp_idx]));
    }
    syndromes[i] = syndrome;
  }

  // Check if errors exist in packet
  bool errors = false;
  for (uint8_t syndrome : syndromes) {
    if (syndrome) {
      errors = true;
      break;
    }
  }

  // If no errors exist, return the data
  if (!errors) {
    return data_copy;
  }

  // Berlekamp-Massey algorithm
  // Reference Used:
  // https://en.wikipedia.org/wiki/Berlekamp%E2%80%93Massey_algorithm
  std::vector<uint8_t> error_locator_poly(1, 1);
  std::vector<uint8_t> old_locator_poly(1, 1);
  uint8_t num_errors = 0;

  for (size_t i = 0; i < parity_size; ++i) {
    // Append 0 to old_locator_poly at each iteration, matching JS: oldLoc =
    // [...oldLoc, 0x00]
    old_locator_poly.push_back(0);

    // Calculate discrepancy delta
    uint8_t delta = polynomial_convolution(error_locator_poly, syndromes, i);

    // If there is a discrepancy
    if (delta != 0) {
      // If the discrepancy is greater than the current num_errors
      if (2 * num_errors > i) {
        // Step 4: Update the error locator polynomial
        error_locator_poly = add_polynomials(
            error_locator_poly, scale_polynomial(old_locator_poly, delta));
      } else {
        // Step 5: temporary copy
        std::vector<uint8_t> temp_locator_poly = error_locator_poly;

        error_locator_poly = add_polynomials(
            error_locator_poly, scale_polynomial(old_locator_poly, delta));
        old_locator_poly =
            scale_polynomial(temp_locator_poly, divide(1, delta));
        // Update the number of errors
        num_errors = i + 1 - num_errors;
      }
    }
  }

  // Chien Search Algorithm (used to find where the errors are)
  std::vector<uint8_t> error_positions;

  for (size_t i = 0; i < 255; ++i) {
    if (evaluate_polynomial(error_locator_poly, i) == 0) {
      error_positions.push_back(LOGARITHM_TABLE[divide(1, i)]);
    }
  }

  // Check if we found the expected number of errors
  if (error_positions.size() != num_errors) {
    // If these aren't equal, then there are too many errors
    // in the packet to correct.
    return std::nullopt;
  }

  // Forney Algorithm (used to find the error values)

  // Reverse syndromes then multiply by error locator
  std::reverse(syndromes.begin(), syndromes.end());
  auto s_times_lambda = multiply_polynomials(syndromes, error_locator_poly);
  std::reverse(syndromes.begin(), syndromes.end());

  // Omega(x) in Forney
  std::vector<uint8_t> omega(s_times_lambda.end() - parity_size,
                             s_times_lambda.end());

  // Lambda'(x) in Forney
  std::vector<uint8_t> lambda_prime(error_locator_poly.size() - 1, 0);
  for (size_t i = error_locator_poly.size() - 2;
       i < error_locator_poly.size() - 1; i -= 2) {
    lambda_prime[i] = error_locator_poly[i];
  }

  // For each error, find the magnitude of the error and correct it
  // Resource used:
  // https://www.diva-portal.org/smash/get/diva2:833161/FULLTEXT01.pdf
  std::vector<uint8_t> corrected_data = data;
  for (const auto position : error_positions) {
    auto X_k = divide(1, EXPONENTIAL_TABLE[position]);
    auto omega_X_k = evaluate_polynomial(omega, X_k);
    auto lambda_prime_X_k = evaluate_polynomial(lambda_prime, X_k);

    auto error_magnitude = divide(omega_X_k, lambda_prime_X_k);

    // Fix the error
    corrected_data[data.size() - position - 1] =
        add(corrected_data[data.size() - position - 1], error_magnitude);
  }

  // Don't return parity bits with data
  return std::vector<uint8_t>(corrected_data.begin(),
                              corrected_data.begin() + k);
}

std::optional<std::vector<uint8_t>>
decode_packet(const std::vector<uint8_t> &data, const RSCode &rscode) {
  // Unpack the parameters
  auto &[n, k] = rscode;
  // Check for invalid block size
  if (k > n || k == 0 || n > 255) {
    throw std::runtime_error(
        "Invalid block parameters: k must be > 0 and <= n, n must be <= 255");
  }

  // Calculate parity size
  // uint8_t parity_size = n - k;

  // Calculate total number of blocks based on data size
  size_t total_bytes = data.size();
  if (total_bytes % n != 0 || data.size() == 0) {
    std::cerr << "Invalid encoded packet size: not a multiple of n"
              << std::endl;
    return std::nullopt;
  }
  size_t num_blocks = total_bytes / n;

  // Result vector to hold all decoded data blocks
  std::vector<uint8_t> result;
  result.reserve(num_blocks * k); // Maximum possible size

  // Process each block separately
  for (size_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
    // Extract the current block (data + parity)
    size_t block_start = block_idx * n;
    std::vector<uint8_t> block(data.begin() + block_start,
                               data.begin() + block_start + n);

    // Decode this block
    auto decoded_block = decode_block(block, rscode);
    if (!decoded_block) {
      // If any block cannot be decoded, entire packet is considered corrupted
      std::cerr << "Failed to decode block " << block_idx << std::endl;
      return std::nullopt;
    }

    // Append successful block to result
    result.insert(result.end(), decoded_block->begin(), decoded_block->end());
  }

  // Remove padding zeros if the original data size wasn't a multiple of k
  // We can't know the exact original size, so we just remove trailing zeros
  while (!result.empty() && result.back() == 0) {
    result.pop_back();
  }

  return result;
}
} // namespace reed_solomon