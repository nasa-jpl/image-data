#ifndef RSVP_IMAGE_DATA_IMAGE_DATA_H
#define RSVP_IMAGE_DATA_IMAGE_DATA_H

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace rsvp
{

    /**
     * @brief A class to read and store image data.
     * formats.
     *
     * This is a virtual base class; subclasses are defined for a variety of
     * image formats (Vicar, PGM, csv), data transformation types (translation,
     * offset and scaling), and compositing (compData).
     */
    class ImageData
    {

    private:
        int alpha_band;
        bool interpolate;

    protected:
        ImageData();

    public:
        /**
         * @brief A static factory method to build an ImageData object from
         * any supported file.
         *
         * @param[in] filename An absolute filepath to the image file to be
         * opened
         *
         * @return A pointer to a newly created ImageData. This object
         * should be deleted by the caller after use.
         */
        static std::shared_ptr<ImageData> read(const std::string &filename);

        /**
         * @brief Enable or disable interpolation for the image data.
         *
         * @param[in] enable Whether to enable or disable interpolation
         */
        virtual void set_interpolating(bool enable);

        /**
         * @brief Get whether or not interpolation is enabled for the image
         * data.
         *
         * @return True if interpolation is enabled, false otherwise.
         */
        virtual int get_interpolating() const;

        /**
         * @brief Set the image band associated with the alpha blending value.
         *
         * @param[in] band The alpha image band
         */
        virtual void set_alpha_band(int band);

        /**
         * @brief Get the image band associated with the alpha blending value.
         *
         * @return The alpha image band
         */
        virtual int get_alpha_band() const;

        /**
         * @brief Return the number of bands of data in this image.
         *
         * @return The number of bands of data in this image.
         */
        virtual int get_bands() const = 0;

        /**
         * @brief Get the uninterpolated pixel band value as a double.
         *
         * The indexing standard used throughout these classes is (x, y)
         * where (0, 0) is the upper-left pixel, x-values increase moving
         * rightward, and y-values increase moving downward. Images may
         * have multiple data bands (e.g. red/green/blue or data/alpha).
         *
         * @param[out] value   Reference to be filled with image data from (x,
         * y, band)
         * @param[in]  x       The "x-like" coordinate of the pixel of interest
         * @param[in]  y       The "y-like" coordinate of the pixel of interest
         * @param[in]  band    The band of the pixel to access
         *
         * @return true if (x, y, band) was within bounds and the result is
         * valid
         */
        virtual bool
        get_pixel_double(double &value, int x, int y, int band) const = 0;

        /**
         * @brief Get the interpolated pixel band value as a double.
         *
         * Subclasses may implement interpolation in different ways -
         * nearest-neighbor, last data wins, bilinear interpolation, etc.
         *
         * @param[out] value  An interpolated computation of the data at * (x,
         * y, band) in double format.
         * @param[in] x       The "x-like" coordinate of the pixel of interest
         * @param[in] y       The "y-like" coordinate of the pixel of interest
         * @param[in] band    The band of the pixel to access
         *
         * @return true if (x, y, band) was within bounds and the result is
         * valid
         */
        virtual bool get_interpolated_pixel_double(double &value,
                                                   double x,
                                                   double y,
                                                   int band) const;

        /**
         * @brief Get the uninterpolated pixel band value as an integer.
         *
         * The indexing standard used throughout these classes is (x, y)
         * where (0, 0) is the upper-left pixel, x-values increase moving
         * rightward, and y-values increase moving downward. Images may
         * have multiple data bands (e.g. red/green/blue or data/alpha).
         *
         * @param[out] value  The data at (x, y, band) in int format.
         * @param[in]  x      The "x-like" coordinate of the pixel of interest
         * @param[in]  y      The "y-like" coordinate of the pixel of interest
         * @param[in]  band   The band of the pixel to access
         *
         * @return true if (x, y, band) was within bounds and the result is
         * valid
         */
        virtual bool get_pixel_int(int &value, int x, int y, int band) const;

        /**
         * @brief Get the interpolated pixel band value as a double.
         *
         * Subclasses may implement interpolation in different ways -
         * nearest-neighbor, last data wins, bilinear interpolation, etc.
         *
         * @param[out] value  The interpolated data at (x, y, band) in int
         * format.
         * @param[in] x       The "x-like" coordinate of the pixel of interest
         * @param[in] y       The "y-like" coordinate of the pixel of interest
         * @param[in] band    The band of the pixel to access
         *
         * @return An interpolated computation of the data at (x, y, band)
         * in integer format.
         */
        virtual bool get_interpolated_pixel_int(int &value,
                                                double x,
                                                double y,
                                                int band) const;

        /**
         * @brief Get the number of samples in each row.
         *
         * @return The number of samples per row.
         */
        virtual int get_width() const
        {
            // Default unless overridden
            return 0;
        }

        /**
         * @brief Get the number of rows of data.
         *
         * @return The number of rows of data.
         */
        virtual int get_height() const
        {
            // Default unless overridden
            return 0;
        }

        /**
         * Get the raw pixel data for this image
         * @param color the encoding of output data:
         *          - BAYER  (RGGB)
         *          - PANCHROMATIC  (average of all bands)
         *          - single band encodings:
         *              - RED
         *              - GREEN
         *              - BLUE
         *          - RGB
         * @return through the data parameter
         */
        void get_image_data(uint32_t *data_ptr,
                            const std::string &color,
                            uint8_t &pix_bytes) const;

        virtual ~ImageData();
    };
}

#endif
