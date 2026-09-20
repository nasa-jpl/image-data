#include "pgm_data.h"

#include <cctype>
#include <fstream>
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
            throw std::runtime_error("Invalid PGM file");
        }

        const size_t data_size = static_cast<size_t>(result->height) *
            result->width * result->pixel_byte_count;

        result->pixels.resize(data_size);
        pgm_file.read(reinterpret_cast<char *>(result->pixels.data()),
                      static_cast<std::streamsize>(data_size));

        return result;
    }
}
