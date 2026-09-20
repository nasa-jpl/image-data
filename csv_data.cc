#include "csv_data.h"

#include <stdexcept>

#include <cstdlib>
#include <fstream>


namespace rsvp
{

    CSVData::CSVData() = default;

    CSVData::~CSVData() = default;

    std::shared_ptr<CSVData> CSVData::read_csv(const std::string &filename)
    {
        std::ifstream csv_file(filename, std::ifstream::in);

        if (not csv_file.is_open())
        {
            throw std::runtime_error("Could not open CSV file: " + filename);
        }

        // Follow the ENav code as close as possible to make sure we are
        // interpreting the CSV the same way. This includes data types.

        std::shared_ptr<CSVData> result(new CSVData());

        std::string str_line;
        while (std::getline(csv_file, str_line))
        {
            std::vector<double> row;

            // A field is whatever lies between commas. A field that is not a
            // number reads as 0, and a trailing comma does not add a field.
            for (size_t start = 0; start < str_line.size();)
            {
                row.push_back(std::strtod(str_line.c_str() + start, nullptr));

                const size_t comma = str_line.find(',', start);
                if (comma == std::string::npos)
                {
                    break;
                }

                start = comma + 1;
            }

            result->data_array.push_back(std::move(row));
        }

        if (result->data_array.empty() or result->data_array.at(0).empty())
        {
            throw std::runtime_error("CSV file " + filename + " is empty");
        }

        const size_t yi_size = result->data_array.at(0).size();

        for (std::vector<std::vector<double>>::const_iterator iter =
                 result->data_array.begin();
             iter != result->data_array.end();
             ++iter)
        {
            if (iter->size() != yi_size)
            {
                const std::string row =
                    std::to_string(iter - result->data_array.begin());
                const std::string row_size = std::to_string(iter->size());
                throw std::runtime_error(
                    "CSV file not rectangular! Row 0 has " +
                    std::to_string(yi_size) + " elements and Row " + row +
                    " has " + row_size + " elements!");
            }
        }

        return result;
    }

    int CSVData::get_width() const
    {
        if (!data_array.empty())
        {
            return static_cast<int>(data_array.at(0).size());
        }
        else
        {
            return 0;
        }
    }

    int CSVData::get_height() const
    {
        return static_cast<int>(data_array.size());
    }

    // Return an exact pixel value as a double
    bool CSVData::get_pixel_double(double &value,
                                   const int x,
                                   const int y,
                                   const int /*band*/) const
    {
        if (y < 0 || static_cast<unsigned>(y) >= data_array.size() || x < 0 ||
            static_cast<unsigned>(x) >= data_array.at(0).size())
        {
            return false;
        }
        else
        {
            value = data_array[y][x];
            return true;
        }
    }
}
