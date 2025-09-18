#ifndef IMAGE_DATA_PLATFORM_H
#define IMAGE_DATA_PLATFORM_H

#include <stdexcept>
#include <string>


namespace rsvp
{
    inline uint16_t byte_swap_16(uint16_t x)
    {
        return static_cast<uint16_t>((x & 0x00FF) << 8 | (x & 0xFF00) >> 8);
    }

    inline uint32_t byte_swap_32(uint32_t x)
    {
        x = (x & 0x0000FFFF) << 16 | (x & 0xFFFF0000) >> 16;
        x = (x & 0x00FF00FF) << 8 | (x & 0xFF00FF00) >> 8;
        return x;
    }

    inline uint64_t byte_swap_64(uint64_t x)
    {
        x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
        x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
        x = (x & 0x00FF00FF00FF00FF) << 8 | (x & 0xFF00FF00FF00FF00) >> 8;
        return x;
    }

    /**
        * Swaps an arbitrary number of bytes in memory.
        * Supports byte-counts:
        *      1 (nop), 2, 4, 8
        * @param output Write the result here
        * @param input Read the input from here
        * @param byte_count number of bytes to swap
        */
    inline void byte_swap(unsigned char *output,
                            const unsigned char *input,
                            int byte_count)
    {
        switch (byte_count)
        {
        case 1:
            output[0] = input[0];
            break;
        case 2:
            output[0] = input[1];
            output[1] = input[0];
            break;
        case 4:
            output[0] = input[3];
            output[1] = input[2];
            output[2] = input[1];
            output[3] = input[0];
            break;
        case 8:
            output[0] = input[7];
            output[1] = input[6];
            output[2] = input[5];
            output[3] = input[4];
            output[4] = input[3];
            output[5] = input[2];
            output[6] = input[1];
            output[7] = input[0];
            break;
        default:
            throw std::runtime_error("Invalid byte count for swap" +
                                    std::to_string(byte_count));
        }
    }

    /**
        * Perform a byte-swap of arbitrary size inplace
        * @param memory memory to swap
        * @param byte_count number of bytes to swap
        */
    inline void byte_swap_inplace(void *memory, int byte_count)
    {
        auto *mem_16 = reinterpret_cast<uint16_t *>(memory);
        auto *mem_32 = reinterpret_cast<uint32_t *>(memory);
        auto *mem_64 = reinterpret_cast<uint64_t *>(memory);

        switch (byte_count)
        {
        case 1:
            break;
        case 2:
            *mem_16 = byte_swap_16(*mem_16);
            break;
        case 4:
            *mem_32 = byte_swap_32(*mem_32);
            break;
        case 8:
            *mem_64 = byte_swap_64(*mem_64);
            break;
        default:
            throw std::runtime_error("Invalid byte count for swap" +
                                    std::to_string(byte_count));
        }
    }
}


#endif // IMAGE_DATA_PLATFORM_H
