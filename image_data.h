#ifndef RSVP_IMAGE_DATA_IMAGE_DATA_H
#define RSVP_IMAGE_DATA_IMAGE_DATA_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rsvp
{

    /**
     * @brief The extension of a path, lower-cased and without the dot.
     *
     * @return The empty string if the path has no extension.
     */
    std::string get_file_extension(const std::string &path);

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
     * @brief The extent of an image's pixels.
     *
     * Bounds run over the pixel centers, in the coordinates the image's
     * lookups take: pixel indices for a bare image, world coordinates
     * (meters) for a terrain placed by a transform.
     */
    struct TerrainBounds
    {
        bool valid = false;     ///< Whether the bounds are valid
        double min_x = 0.0;     ///< Minimum X coordinate
        double max_x = 0.0;     ///< Maximum X coordinate
        double min_y = 0.0;     ///< Minimum Y coordinate
        double max_y = 0.0;     ///< Maximum Y coordinate

        /**
         * How far outside the bounds a lookup can still land on a pixel: one
         * pixel's pitch, in the same coordinates as the bounds. A lookup
         * further out than this along either axis finds nothing.
         *
         * Zero means unknown, and rules nothing out.
         */
        double pixel_reach = 0.0;

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
         * @brief Check whether a lookup at (x, y) could land on a pixel.
         *
         * This is the test a composite skips a child by, so it errs toward
         * yes: a point is only ruled out when the bounds and the reach are
         * both known and it lies beyond them.
         *
         * @param[in] x      The "x-like" coordinate to test
         * @param[in] y      The "y-like" coordinate to test
         * @param[in] margin Slack beyond the reach, as for `contains`
         *
         * @return false if a lookup at (x, y) certainly finds nothing. true
         * means only that it might.
         */
        bool could_reach(double x, double y, double margin = 0.0) const
        {
            return !valid || pixel_reach <= 0.0 ||
                contains(x, y, pixel_reach + margin);
        }

        /**
         * @brief Union this bounds with another bounds.
         *
         * A point within either's reach of the union is within the larger of
         * the two reaches of it, and a reach that is unknown on either side
         * stays unknown.
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

            pixel_reach = (pixel_reach > 0.0 && other.pixel_reach > 0.0)
                ? std::max(pixel_reach, other.pixel_reach)
                : 0.0;
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

        /**
         * @brief The four pixels a bilinear sample at (x, y) draws on, and
         * the weight each is owed.
         *
         * The corners are (x0, y0), (x1, y0), (x0, y1) and (x1, y1), with the
         * weights in that order. A corner whose weight is zero is collapsed
         * onto its neighbour, so the four are always inside the image
         * whenever (x, y) is.
         */
        struct BilinearCorners
        {
            int x0;
            int y0;
            int x1;
            int y1;
            double weight_ul;
            double weight_ur;
            double weight_ll;
            double weight_lr;
        };

        /**
         * @brief Work out the corners and weights of a bilinear sample.
         *
         * Shared by the default `get_interpolated_pixel_double` and by images
         * that can read the corners straight out of memory.
         */
        static BilinearCorners bilinear_corners(double x, double y)
        {
            // Converting a double to an int is undefined unless the value
            // fits, and a target that saturates lands the far corner at
            // INT_MAX + 1, which wraps to the near side of every bounds
            // check. A coordinate that far out - or a NaN, which fails every
            // comparison - cannot be in any image, so send it to a corner
            // that is certainly outside one instead of casting it.
            //
            // The limits leave room for the +1 on the far corner and the
            // decrement on the near one.
            if (!(x > -2147483647.0 && x < 2147483646.0))
            {
                x = -1.0;
            }

            if (!(y > -2147483647.0 && y < 2147483646.0))
            {
                y = -1.0;
            }

            // We sample at (x0, y0) and (x0 + 1, y0 + 1), so those two must
            // span the input, which means rounding down rather than toward
            // zero. Getting that wrong for coordinates in (-1, 0) - leaving
            // the corner at 0 - is what used to mirror the interpolation
            // about the image edge instead of reporting the coordinate out of
            // bounds.
            //
            // Done by hand rather than with std::floor: this is the terrain
            // settling hot path, and on 32-bit x86 without SSE4.1 there is no
            // instruction to inline std::floor to, so it compiles to a libm
            // call.
            int x0 = static_cast<int>(x);
            int y0 = static_cast<int>(y);

            if (x < x0)
            {
                x0--;
            }

            if (y < y0)
            {
                y0--;
            }

            const double frac_x = x - x0;
            const double frac_y = y - y0;

            // When a fraction is zero the far corner has zero weight, so it
            // must not be required to exist - otherwise a coordinate landing
            // exactly on the last row or column of an image would fail for
            // want of a neighbor it does not need. Fold that in by collapsing
            // the far corner onto the near one, which keeps the four fetches
            // unconditional.
            //
            // Two cheaper-looking alternatives measure worse, so leave this
            // alone: branching on the fractions costs a pair of unpredictable
            // branches per call (and comparing a double against zero costs
            // two branches, not one), and letting a weightless fetch fail
            // instead stops the compiler short-circuiting the weight test.
            BilinearCorners corners;
            corners.x0 = x0;
            corners.y0 = y0;
            corners.x1 = (frac_x > 0.0) ? x0 + 1 : x0;
            corners.y1 = (frac_y > 0.0) ? y0 + 1 : y0;
            corners.weight_ul = (1.0 - frac_x) * (1.0 - frac_y);
            corners.weight_ur = frac_x * (1.0 - frac_y);
            corners.weight_ll = (1.0 - frac_x) * frac_y;
            corners.weight_lr = frac_x * frac_y;
            return corners;
        }

        /**
         * @brief Round to the nearest integer, halves away from zero.
         *
         * Used both to snap a coordinate to a whole pixel for uninterpolated
         * lookups and to round a value for the integer accessors.
         *
         * A value that does not fit in an int saturates - and a NaN, which
         * fails every comparison, comes out as INT_MIN - since converting it
         * would be undefined. As a coordinate, either end is outside every
         * image.
         */
        static int round_to_int(double value)
        {
            if (value > 0.0)
            {
                return value < 2147483647.0 ? static_cast<int>(value + 0.5) :
                                              2147483647;
            }

            return value > -2147483648.0 ? static_cast<int>(value - 0.5) :
                                           (-2147483647 - 1);
        }


        /**
         * @brief The bounds of this image's own pixel grid, in pixel indices.
         *
         * For an image that holds a grid of pixels to return from
         * `get_bounds`: the pixel centers, reaching one pixel past them.
         *
         * @return Invalid bounds if the grid is empty.
         */
        TerrainBounds pixel_grid_bounds() const
        {
            TerrainBounds bounds;

            if (get_width() < 1 || get_height() < 1)
            {
                return bounds;
            }

            bounds.valid = true;
            bounds.max_x = get_width() - 1;
            bounds.max_y = get_height() - 1;
            bounds.pixel_reach = 1.0;
            return bounds;
        }

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
         * A composite caches which band of each child it blends by, and only
         * learns that this has changed through `invalidate_geometry`, which
         * this calls. An override must therefore call this as well as
         * whatever else it does.
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
         * @brief Get the interpolated values of several bands at one point.
         *
         * A composite wants a child's data and its alpha at the same point,
         * and asking for them a band at a time repeats the walk down the
         * child's chain of wrappers - the inverse transform, the offset, the
         * search for the corners - once per band. This makes the walk once:
         * a wrapper transforms the point and hands the whole request down,
         * and an image reads every band from the same corners.
         *
         * The default asks for the bands one at a time.
         *
         * @param[out] values The interpolated value of each of `bands`, in
         * the same order. Unspecified when this returns false.
         * @param[in]  bands  The bands to sample
         * @param[in]  count  How many bands there are
         * @param[in]  x      The "x-like" coordinate of the pixel of interest
         * @param[in]  y      The "y-like" coordinate of the pixel of interest
         *
         * @return true if (x, y) was within bounds and every band was valid
         */
        virtual bool get_interpolated_bands_double(double *values,
                                                   const int *bands,
                                                   int count,
                                                   double x,
                                                   double y) const;

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
         * @brief Where this image's pixels are, in the coordinates its
         * lookups take.
         *
         * Bounds run over the pixel centers. For a bare image they are pixel
         * indices; for a terrain placed by a transform they are world
         * coordinates, in meters; for a composite they are the union of its
         * children's. Whichever it is, they are in the coordinates a lookup on
         * this image takes, which is what lets a composite skip a child by
         * them. Where a VICAR file's labels say it sits in the world is a
         * different question, answered by `VicarData::get_map_bounds`.
         *
         * @return Invalid bounds unless overridden. An image holding a pixel
         * grid of its own reports it with `pixel_grid_bounds`. The default is
         * left invalid rather than derived from `get_width` and `get_height`
         * because a wrapper that forwards those without overriding this would
         * then report its stored image's grid in a frame its own lookups do
         * not take, and a composite would skip it by bounds that are not where
         * its pixels are. An image with invalid bounds is never skipped.
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
