#include "csv_data.h"

#include <stdexcept>

#include <fstream>
#include <iostream>
#include <sstream>


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
            std::istringstream s(str_line);
            std::vector<double> c;
            std::string str_field;
            while (std::getline(s, str_field, ','))
            {
                std::stringstream ss;
                ss << str_field;
                double value = 0;
                ss >> value;
                c.push_back(value);
            }
            result->data_array.push_back(c);
        }

        if (result->data_array.empty() or result->data_array.at(0).empty())
        {
            throw std::runtime_error("CSV file " + filename + " is empty");
        }

        const size_t xi_size = result->data_array.size();
        const size_t yi_size = result->data_array.at(0).size();

        if (xi_size < 1 or yi_size < 1)
        {
            throw std::runtime_error("CSV file too small it seems to be " +
                                     std::to_string(xi_size) + " by " +
                                     std::to_string(yi_size));
        }

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
