#include "utils.h"

#include <cstdint>
#include <chrono>
#include <string>

namespace util
{
    uint16_t computeInternetChecksum(const std::string &data)
    {
        uint32_t sum = 0;
        size_t length = data.size();
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());

        // Sum up 16-bit words
        for (size_t i = 0; i + 1 < length; i += 2)
        {
            uint16_t word = (ptr[i] << 8) | ptr[i + 1];
            sum += word;
            if (sum > 0xFFFF)
            { // Do the wrap-around Adding
                sum = (sum & 0xFFFF) + 1;
            }
        }

        // If the packet is an odd number of bytes, add the last one with zeroes padded onto it
        if (length % 2 == 1)
        {
            uint16_t word = ptr[length - 1] << 8;
            sum += word;
            if (sum > 0xFFFF)
            {
                sum = (sum & 0xFFFF) + 1;
            }
        }

        // One's complement
        return static_cast<uint16_t>(~sum);
    }

    bool validInternetChecksum(const std::string &packet)
    {
        // Size Guard
        if (packet.size() < 2)
            return false;

        // If the checksum is 0x0000, then it is valid
        return computeInternetChecksum(packet) == 0x0000;
    }

    uint64_t current_time()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

}