#include "error_correction.h"
#include "protocols.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

class ReedSolomonTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ReedSolomonTest, ComputeParityBasic) {
  // Simple test case with known values
  std::string data = "hello";
  auto data_vec = util::struct_to_bytes(data);
  RSCode rscode(7, 5);

  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  // Check that parity has the expected size
  ASSERT_EQ(parity.size(), rscode.n - rscode.k);
}

TEST_F(ReedSolomonTest, ComputeParityZeroData) {
  // Test with all zeros data
  std::vector<uint8_t> data_vec = {0, 0, 0, 0, 0};
  RSCode rscode(10, 5);

  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  // Check that all parity bits are zeros
  for (const auto &bit : parity) {
    EXPECT_EQ(bit, 0);
  }
}

TEST_F(ReedSolomonTest, ComputeParityInvalidParams) {
  std::string data = "test";
  auto data_vec = util::struct_to_bytes(data);
  RSCode rscode(5, 4);
  rscode.n = 3; // Invalid parameter

  // k > n, should throw an exception
  EXPECT_THROW(reed_solomon::compute_parity(data_vec, rscode),
               std::runtime_error);
}

TEST_F(ReedSolomonTest, ComputeParityConsistency) {
  // Test that the same input produces the same output
  std::string data = "this is a test of parity consistency";
  auto data_vec = util::struct_to_bytes(data);
  RSCode rscode(40, 37);

  auto parity1 = reed_solomon::compute_parity(data_vec, rscode);
  auto parity2 = reed_solomon::compute_parity(data_vec, rscode);

  ASSERT_EQ(parity1.size(), parity2.size());
  for (size_t i = 0; i < parity1.size(); i++) {
    EXPECT_EQ(parity1[i], parity2[i]);
  }
}

TEST_F(ReedSolomonTest, ComputeParityDifferentSizes) {
  std::string data = "hello";
  auto data_vec = util::struct_to_bytes(data);

  // Test with different n and k values
  auto parity1 = reed_solomon::compute_parity(data_vec, RSCode(10, 5));
  auto parity2 = reed_solomon::compute_parity(data_vec, RSCode(12, 5));

  ASSERT_EQ(parity1.size(), 5);
  ASSERT_EQ(parity2.size(), 7);

  // Different n and k should produce different parity sizes
  EXPECT_NE(parity1.size(), parity2.size());
}

TEST_F(ReedSolomonTest, DecodePacketNoErrors) {
  // Test decoding a packet with no errors
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(10, 5);

  // Generate parity bits and create an encoded packet
  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  // Create complete packet (data + parity)
  std::vector<uint8_t> encoded_packet = data_vec;
  encoded_packet.insert(encoded_packet.end(), parity.begin(), parity.end());

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Check that the decoded data matches the original
  ASSERT_EQ(decoded->size(), data_vec.size());
  for (size_t i = 0; i < data_vec.size(); i++) {
    EXPECT_EQ((*decoded)[i], data_vec[i]);
  }
}

TEST_F(ReedSolomonTest, DecodePacketSingleError) {
  // Test decoding a packet with a single error
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(10, 5);

  // Generate parity bits and create an encoded packet
  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  std::vector<uint8_t> encoded_packet = data_vec;
  encoded_packet.insert(encoded_packet.end(), parity.begin(), parity.end());

  // Introduce a single error (flip a bit in the first byte)
  encoded_packet[0] ^= 0x01;

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Check that the decoded data matches the original (error was corrected)
  ASSERT_EQ(decoded->size(), data_vec.size());
  for (size_t i = 0; i < data_vec.size(); i++) {
    EXPECT_EQ((*decoded)[i], data_vec[i]);
  }
}

TEST_F(ReedSolomonTest, DecodePacketMultipleErrors) {
  // Test decoding a packet with multiple errors (within correction capability)
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(10, 5);

  // Reed-Solomon can correct up to (n-k)/2 errors, which is (10-5)/2 = 2 errors

  // Generate parity bits and create an encoded packet
  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  std::vector<uint8_t> encoded_packet = data_vec;
  encoded_packet.insert(encoded_packet.end(), parity.begin(), parity.end());

  // Introduce two errors
  encoded_packet[0] ^= 0x01; // Error in data
  encoded_packet[6] ^= 0x10; // Error in parity

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Check that the decoded data matches the original (errors were corrected)
  ASSERT_EQ(decoded->size(), data_vec.size());
  for (size_t i = 0; i < data_vec.size(); i++) {
    EXPECT_EQ((*decoded)[i], data_vec[i]);
  }
}

TEST_F(ReedSolomonTest, DecodePacketTooManyErrors) {
  // Test decoding a packet with too many errors to correct
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(10, 5);

  // Reed-Solomon can correct up to (n-k)/2 errors, which is (10-5)/2 = 2 errors
  // We'll introduce 3 errors, which should be too many to correct

  // Generate parity bits and create an encoded packet
  auto parity = reed_solomon::compute_parity(data_vec, rscode);

  std::vector<uint8_t> encoded_packet = data_vec;
  encoded_packet.insert(encoded_packet.end(), parity.begin(), parity.end());

  // Introduce three errors
  encoded_packet[0] ^= 0x01;
  encoded_packet[2] ^= 0x04;
  encoded_packet[7] ^= 0x20;

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Decoding should fail due to too many errors
  EXPECT_FALSE(decoded.has_value());
}

TEST_F(ReedSolomonTest, DecodePacketInvalidParams) {
  // Test with invalid parameters
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(8, 5);

  auto encoded_packet = reed_solomon::encode_packet(data_vec, rscode);

  // Now, invalid parameters
  rscode.n = 3;

  // k > n should throw an exception
  EXPECT_THROW(reed_solomon::decode_packet(encoded_packet, rscode),
               std::runtime_error);
}

TEST_F(ReedSolomonTest, DecodePacketInvalidSize) {
  // Test with invalid packet size
  std::vector<uint8_t> data_vec = {'h', 'e', 'l', 'l', 'o'};
  RSCode rscode(10, 5);

  // Create a packet that's too small
  std::vector<uint8_t> short_packet = {0x01, 0x02, 0x03}; // Only 3 bytes

  // Decoding should fail due to packet being too small
  auto decoded = reed_solomon::decode_packet(short_packet, rscode);
  EXPECT_FALSE(decoded.has_value());

  // Test with empty packet
  std::vector<uint8_t> empty_packet = {};
  auto decoded_empty = reed_solomon::decode_packet(empty_packet, rscode);
  EXPECT_FALSE(decoded_empty.has_value());
}

TEST_F(ReedSolomonTest, DecodePacketErrorsInDataAndParity) {
  // Test with errors in both data and parity sections
  std::string data = "Hello World!";
  RSCode rscode(19, 13);

  // Generate parity bits and create an encoded packet
  auto encoded_packet = reed_solomon::encode_packet(data, rscode);

  // Introduce errors in data and parity (but still within correction
  // capability)
  encoded_packet[2] ^= 0x04;  // Error in data
  encoded_packet[12] ^= 0x08; // Error in parity

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Check that the decoded data matches the original (errors were corrected)
  ASSERT_EQ(decoded->size(), data.size()); // +1 for null terminator
  for (size_t i = 0; i < data.size() - 1; i++) {
    EXPECT_EQ((*decoded)[i], data[i]);
  }
}

TEST_F(ReedSolomonTest, DecodePacketLargeData) {
  // Test with a larger data set
  std::string original_data =
      "According to all known laws of aviation, there is no way a bee should be able to fly. \
    Its wings are too small to get its fat little body off the ground. The bee, of course, flies anyway because bees \
    don't carewhat humans think is impossible. Yellow, black. Yellow, black. Yellow, black. Yellow, black. Ooh, black and yellow! \
    Let's shake it up a little. Barry! Breakfast is ready! Ooming! Hang on a second. Hello? - Barry? - Adam? - Oan you believe this is happening? \
    - I can't. I'll pick you up. Looking sharp. Use the stairs. Your father paid good money for those. Sorry. I'm excited. Here's the graduate. \
    We're very proud of you, son. A perfect report card, all B's.";
  RSCode rscode(63, 31);

  // Generate parity bits and create an encoded packet
  auto encoded_packet = reed_solomon::encode_packet(original_data, rscode);

  // Introduce a few errors (within correction capability)
  encoded_packet[5] ^= 0x10;
  encoded_packet[20] ^= 0x04;
  encoded_packet[50] ^= 0x40;
  encoded_packet[61] ^= 0x02; // Error in parity

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded_packet, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Check that the decoded data matches the original
  auto data_vec = util::struct_to_bytes(original_data);
  ASSERT_EQ(decoded->size(), data_vec.size());
  for (size_t i = 0; i < data_vec.size(); i++) {
    EXPECT_EQ((*decoded)[i], data_vec[i]);
  }
}

TEST_F(ReedSolomonTest, EncodeDecodeRoundtrip) {
  // Test end-to-end encode-decode process
  struct TestStruct {
    int id;
    double value;
    char name[10];
  };

  // Create a test struct
  TestStruct original = {42, 3.14159, "test"};

  RSCode rscode(sizeof(TestStruct) + 8, sizeof(TestStruct)); // 8 parity bytes

  // Encode the struct
  auto encoded = reed_solomon::encode_packet(original, rscode);

  // Decode the packet
  auto decoded = reed_solomon::decode_packet(encoded, rscode);

  // Check that decoding was successful
  ASSERT_TRUE(decoded.has_value());

  // Convert back to struct and compare
  TestStruct *result = reinterpret_cast<TestStruct *>(decoded->data());

  EXPECT_EQ(result->id, original.id);
  EXPECT_DOUBLE_EQ(result->value, original.value);
  EXPECT_STREQ(result->name, original.name);
}
