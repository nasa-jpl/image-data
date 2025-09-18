#include "pgm_data.h"

#include "platform.h"
#include <fstream>
#include <iostream>
#include <stdexcept>


namespace rsvp
{
    std::shared_ptr<PGMData> PGMData::read_pgm(const std::string &filename)
    {
        std::ifstream pgm_file(filename, std::ios::binary);

        // If we failed to open the file, abort
        if (!pgm_file.is_open())
        {
            throw std::runtime_error("Unable to open pgm file: " + filename);
        }

        // Get the first space-separated token in the file
        std::string magic_number;
        pgm_file >> magic_number;

        if (magic_number != "P5")
        {
            pgm_file.close();
            throw std::runtime_error("Invalid PGM file: First bytes were `" +
                                     magic_number + "`, not `P5`!");
        }

        // Get the width, height, and maximum value, ignoring any any comment
        // lines
        std::string token;
        std::vector<std::string> tokens;

        while (tokens.size() < 3)
        {
            pgm_file >> token;

            // Ignore anything from a # to the next newline
            if (token.find('#') == 0)
            {
                std::getline(pgm_file, token);
                continue;
            }
            else
            {
                tokens.push_back(token);
            }
        }

        std::shared_ptr<PGMData> result(new PGMData());
        result->width = std::stoi(tokens[0]);
        result->height = std::stoi(tokens[1]);
        result->maximum_value = std::stoi(tokens[2]);
        result->pixel_byte_count = (result->maximum_value < 256) ? 1 : 2;

        // We're now past the region of potential comments, so strip off
        // a single whitespace character
        int should_be_whitespace = pgm_file.get();

        if (!isspace(should_be_whitespace))
        {
            pgm_file.close();
            throw std::runtime_error("Invalid PGM file");
        }

        // Note how far into the file the data begins
        result->data_offset = static_cast<int>(pgm_file.tellg());

        int data_size =
            (result->height * result->width) * result->pixel_byte_count;

        result->pixel_array_8 = static_cast<uint8_t *>(malloc(data_size));
        result->pixel_array_16 =
            reinterpret_cast<uint16_t *>(result->pixel_array_8);

        pgm_file.read(reinterpret_cast<char *>(result->pixel_array_8),
                      data_size);

        // Close the filestream
        pgm_file.close();

        // Fix pixel_array
        if (result->pixel_byte_count == 1)
        {
            // No need to cast or byte swap the data
            // The pixel_array_8 is good as is
        }
        else if (result->pixel_byte_count == 2)
        {
            // Swap all of the data if need be
            if (result->opposite_endian)
            {
                for (int i = 0; i < result->width * result->height; i++)
                {
                    rsvp::byte_swap_inplace(&result->pixel_array_16[i], 2);
                }
            }
        }
        else
        {
            throw std::runtime_error(
                "Invalid pixel byte count for PGM data: " +
                std::to_string(result->pixel_byte_count));
        }

        return result;
    }

    PGMData::PGMData() :
        width(0),
        height(0),
        maximum_value(0),
        pixel_byte_count(0),
        data_offset(0),
        pixel_array_8(nullptr),
        pixel_array_16(nullptr)
    {
        uint16_t num = 1;
        opposite_endian = (*reinterpret_cast<char *>(&num) == 1);
    }

    bool PGMData::get_pixel_int(int &value,
                                const int sample,
                                const int line,
                                const int /*band*/) const
    {
        // The G in PGM is "gray" - bands don't matter!
        if (line < 0 || line >= height || sample < 0 || sample >= width)
        {
            // Out of bounds
            return false;
        }

        int index = width * line + // Samples in prior lines
            sample;                // Prior samples in current lines

        if (pixel_byte_count == 1)
        {
            value = pixel_array_8[index];
        }
        else
        {
            value = pixel_array_16[index];
        }

        return true;
    }

    PGMData::~PGMData()
    {
        if (pixel_array_8)
        {
            free(pixel_array_8);
        }
    }
}
