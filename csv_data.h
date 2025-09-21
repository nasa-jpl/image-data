#ifndef RSVP_IMAGE_DATA_CSV_DATA_H
#define RSVP_IMAGE_DATA_CSV_DATA_H

#include "image_data.h"


namespace rsvp
{

    /**
     * @brief A class to read data stored in a CSV file.
     *
     * The comma-separated values are assumed to be text strings representing
     * floating-point values. Each row must have the same number of samples.
     * There is only one band of data allowed.
     */
    class CSVData : public ImageData
    {
    private:
        std::vector<std::vector<double> > data_array;

        // Define a private constructor to force use of the factory
        CSVData();

    public:
        /**
         * @brief A static factory method to construct a CSVData from a file.
         *
         * @param filename The absolute filepath to the .csv file.
         *
         * @return A shared pointer to the newly constructed CSVData. Throws an
         * std::runtime_error if an error occurred.
         */
        static std::shared_ptr<CSVData> read_csv(const std::string &filename);

        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const;

        /**
         * @brief Get the number of samples in each row.
         *
         * @return The number of samples per row.
         */
        int get_width() const;

        /**
         * @brief Get the number of rows of data.
         *
         * @return The number of rows of data.
         */
        int get_height() const;

        // CSV files have only a single band.
        virtual int get_bands() const
        {
            return 1;
        }

        ~CSVData();
    };
}

#endif
