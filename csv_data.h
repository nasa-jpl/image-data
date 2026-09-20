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

        bool get_pixel_double(double &value,
                              const int x,
                              const int y,
                              const int band) const override;

        /**
         * @brief Get the number of samples in each row.
         *
         * @return The number of samples per row.
         */
        int get_width() const override;

        /**
         * @brief Get the number of rows of data.
         *
         * @return The number of rows of data.
         */
        int get_height() const override;

        // CSV files have only a single band.
        int get_bands() const override
        {
            return 1;
        }

        /// A CSV is looked up by row and column, so that is where its pixels
        /// are
        TerrainBounds get_bounds() const override
        {
            return pixel_grid_bounds();
        }

        ~CSVData();
    };
}

#endif
