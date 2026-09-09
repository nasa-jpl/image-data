#ifndef RSVP_IMAGE_DATA_IMAGE_DATA_H
#define RSVP_IMAGE_DATA_IMAGE_DATA_H

#include <list>
#include <memory>
#include <string>
#include <vector>

namespace rsvp
{

    /**
     * @brief A counter that changes whenever the placement of any image
     * changes.
     *
     * Deriving the bounds of an image can be expensive - `VicarData` parses
     * them out of its labels - so a container that queries its children's
     * bounds repeatedly wants to cache the result. There is no way for a
     * container to notice that a child has been moved underneath it, though,
     * so instead every operation that moves an image bumps this counter, and a
     * cache is only good for as long as the counter it was computed under.
     *
     * The counter starts at 1, so a cache stamped with 0 is one that has never
     * been computed.
     *
     * @return The current version.
     */
    unsigned long geometry_version();

    /**
     * @brief Invalidate every cache derived from where images sit and how
     * their bands are laid out.
     *
     * Call this from anything that changes the placement, the membership or
     * the band layout of an image: setting a transform, adding an image to a
     * container, or setting an alpha band.
     *
     * @see geometry_version
     */
    void invalidate_geometry();

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
         * @brief Check whether a point falls inside the bounds.
         *
         * Bounds run to the centers of the outermost pixels rather than past
         * them, so a point on the boundary is inside.
         *
         * @param[in] x      The "x-like" coordinate to test
         * @param[in] y      The "y-like" coordinate to test
         * @param[in] margin How far outside the bounds still counts as inside.
         * Bounds and pixel lookups are derived by different routes - labels
         * versus affine transforms - so a caller that must not reject a point
         * the pixels do cover should leave itself a little slack here.
         *
         * @return false if the bounds are invalid, since then nothing is known
         * about what they contain.
         */
        bool contains(double x, double y, double margin = 0.0) const
        {
            return valid && x >= min_x - margin && x <= max_x + margin &&
                y >= min_y - margin && y <= max_y + margin;
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
         * @brief Sample the image with the request clamped to the nearest
         * valid pixel, and report the weight that clamped sample deserves.
         *
         * Tiled mosaics - orbital DEMs in particular - abut without
         * overlapping, so their pixel grids are continuous but each tile's
         * pixels only span `width - 1` times the tile's pitch. There is
         * therefore a one-pixel-wide band along every seam that no single tile
         * can interpolate on its own. This method lets a composite reconstruct
         * that band: each tile along the seam reports the value on its own
         * edge plus the bilinear weight that edge is owed.
         *
         * @param[out] value  The value sampled at the clamped location.
         * @param[out] weight The weight of `value`, in the range (0, 1]. It is
         * 1 when (x, y) is inside the image, and falls off linearly to 0 a
         * pixel outside of it.
         * @param[in] x       The "x-like" coordinate of the pixel of interest
         * @param[in] y       The "y-like" coordinate of the pixel of interest
         * @param[in] band    The band of the pixel to access
         *
         * @return false if (x, y) is more than a pixel outside the image, or
         * if the clamped sample itself is unavailable
         */
        virtual bool get_clamped_pixel_double(
            double &value, double &weight, double x, double y, int band) const;

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
