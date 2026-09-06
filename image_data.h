#ifndef RSVP_IMAGE_DATA_IMAGE_DATA_H
#define RSVP_IMAGE_DATA_IMAGE_DATA_H

#include <list>
#include <memory>
#include <string>
#include <vector>

namespace rsvp
{

    /**
     * @brief A struct to represent the spatial bounds of terrain data.
     *
     * Bounds are specified in world coordinates (meters).
     */
    struct TerrainBounds
    {
        bool valid = false;     ///< Whether the bounds are valid
        double min_x = 0.0;     ///< Minimum X coordinate (meters)
        double max_x = 0.0;     ///< Maximum X coordinate (meters)
        double min_y = 0.0;     ///< Minimum Y coordinate (meters)
        double max_y = 0.0;     ///< Maximum Y coordinate (meters)

        /**
         * @brief Check if the bounds are valid.
         */
        bool is_valid() const
        {
            return valid;
        }

        /**
         * @brief Get the width of the bounds.
         */
        double get_width() const
        {
            return max_x - min_x;
        }

        /**
         * @brief Get the height of the bounds.
         */
        double get_height() const
        {
            return max_y - min_y;
        }

        /**
         * @brief Union this bounds with another bounds.
         */
        void merge(const TerrainBounds &other)
        {
            if (!other.valid)
            {
                return;
            }

            if (!valid)
            {
                *this = other;
                return;
            }

            min_x = std::min(min_x, other.min_x);
            max_x = std::max(max_x, other.max_x);
            min_y = std::min(min_y, other.min_y);
            max_y = std::max(max_y, other.max_y);
        }
    };

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
         * @brief Get the spatial bounds of this terrain data in world
         * coordinates.
         *
         * This method queries the underlying image data format (e.g., VICAR
         * labels) to determine the real-world extent of the data. For composite
         * images, this returns the union of all constituent image bounds.
         *
         * @return TerrainBounds struct containing the spatial extent in meters,
         * or an invalid bounds if the data does not have spatial information.
         */
        virtual TerrainBounds get_bounds() const
        {
            // Default implementation returns invalid bounds
            return TerrainBounds();
        }

        /**
         * @brief Get the distance from a point to the edge of the image.
         *
         * The distance is measured in this image's own pixel units, from the
         * continuous pixel coordinate (x, y) to the boundary of the region in
         * which bilinear interpolation succeeds, i.e. [0, width - 1) x
         * [0, height - 1). Points outside that region return 0.
         *
         * Compositors use this to feather (ramp down) an image's weight near
         * its border so that overlapping images blend without a visible seam.
         *
         * @param[in] x The "x-like" coordinate of the point of interest
         * @param[in] y The "y-like" coordinate of the point of interest
         *
         * @return The distance in pixels, or +infinity if the image size is
         * unknown (width or height is 0), in which case no feathering is
         * applied.
         */
        virtual double get_edge_distance(double x, double y) const;

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
