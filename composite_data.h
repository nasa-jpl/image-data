#ifndef RSVP_IMAGE_DATA_COMPOSITE_DATA_H
#define RSVP_IMAGE_DATA_COMPOSITE_DATA_H

#include "image_data.h"


namespace rsvp
{

    /**
     * @brief A class to composite multiple ImageData objects together.
     */
    class CompositeData : public ImageData
    {

    private:
        // Merging the children's bounds means asking each of them where it is,
        // which for a VicarData means parsing its labels. Too slow to repeat
        // per pixel lookup, so remember the answer for as long as no image has
        // moved.
        mutable TerrainBounds cached_bounds;
        mutable unsigned long cached_bounds_version = 0;

        /**
         * @brief The merged bounds of the children, recomputed only when an
         * image has moved since they were last worked out.
         *
         * Returns a reference because pixel lookups consult this and do not
         * need a copy.
         */
        const TerrainBounds &merged_bounds() const;

    protected:
        std::vector<std::shared_ptr<rsvp::ImageData> > images;

    public:
        /**
         * @brief Construct a new empty CompositeData.
         */
        CompositeData();

        ~CompositeData();

        virtual int get_alpha_band() const override;

        /**
         * @brief Add an image to the composite.
         *
         * @param img The ImageData to add to the composite
         * @param pos The index at which to add the data
         */
        void add_image(const std::shared_ptr<rsvp::ImageData> &img,
                       int pos = -1);

        /**
         * @brief Remove an image from the composite but do _not_ delete it.
         *
         * @param pos The index of the ImageData to remove
         *
         * @return A shared pointer to the removed ImageData.
         */
        std::shared_ptr<rsvp::ImageData> remove_image(int p);

        /**
         * @brief Remove an image from the composite and _do_ delete it.
         *
         * @param pos The index of the ImageData to remove
         *
         * @return true if an ImageData was removed from the list.
         */
        bool delete_image(int p);

        /**
         * @brief Get the number of ImageData objects in the composite.
         *
         * @return The number of ImageData objects in the composite.
         */
        int get_count() const;

        /**
         * @brief Get the number of bands in the composite data.
         *
         * @return The number of bands in the composite data.
         */
        virtual int get_bands() const override;

        /**
         * @brief Get the union of bounds from all images in the composite.
         *
         * @return The combined terrain bounds.
         */
        TerrainBounds get_bounds() const override;

        /**
         * @brief Set interpolation mode for composite and all child images.
         *
         * @param enable true to enable bilinear interpolation, false for nearest-neighbor
         */
        void set_interpolating(bool enable) override;

        /**
         * @brief Blend the edge samples of every child within a pixel of
         * (x, y), and report the largest of their weights.
         *
         * @see ImageData::get_clamped_pixel_double
         */
        bool get_clamped_pixel_double(double &value,
                                      double &weight,
                                      double x,
                                      double y,
                                      int band) const override;

    protected:
        /**
         * @brief Reconstruct a value in the band between abutting images.
         *
         * Tiled DEM mosaics abut without overlapping, so there is a
         * one-pixel-wide band along every seam that no single tile can
         * interpolate - each of the four bilinear corners the band needs lives
         * in a different tile. This blends the edge value of every child
         * within a pixel of (x, y), weighted by how close that edge is.
         *
         * For grids that share a pitch and a phase, which tiles cut from a
         * common mosaic do, the result is exactly the bilinear interpolation
         * the tiles would produce if they were a single image.
         *
         * This is meant as a fallback for the seam band, so callers should try
         * their normal compositing first: inside a tile this returns the same
         * answer, but more expensively.
         *
         * @param[out] value The reconstructed value.
         * @param[in] x      The "x-like" coordinate of the pixel of interest
         * @param[in] y      The "y-like" coordinate of the pixel of interest
         * @param[in] band   The band of the pixel to access
         *
         * @return false if no child is within a pixel of (x, y), which means
         * (x, y) is genuinely outside the composite rather than on a seam.
         */
        bool get_seam_pixel_double(double &value,
                                   double x,
                                   double y,
                                   int band) const;

        /**
         * @brief Cheaply rule out (x, y) being on one of this composite's
         * seams.
         *
         * A seam is a band between two images, so it takes two images to have
         * one, and it lies inside the composite rather than beyond its outer
         * edge. Both are much cheaper to check than the sample-every-child
         * loops that reconstruct a seam, and out-of-terrain lookups are common
         * enough to be worth the check.
         *
         * @param[in] x The "x-like" coordinate of the pixel of interest
         * @param[in] y The "y-like" coordinate of the pixel of interest
         *
         * @return false if (x, y) cannot be on a seam. true means only that it
         * might be.
         */
        bool could_be_on_seam(double x, double y) const;

        /**
         * @brief Work out which band of an image carries its alpha value.
         *
         * Prefers what the image declares. An image that declares nothing
         * falls back on its format: a single-band PGM has no alpha, and a
         * three-band heightmap or terrain classification keeps it in band 2.
         *
         * Every composite here resolves the alpha band through this, so that a
         * seam is judged opaque or transparent by the same rule as the pixels
         * on either side of it.
         *
         * @param[in] image The image to inspect
         *
         * @return The band carrying alpha, or -1 if the image has none.
         */
        static int get_alpha_band_of(const ImageData &image);

        /**
         * @brief Sample one child of the composite for a seam reconstruction.
         *
         * Takes the child's value at the point on its edge nearest to (x, y)
         * along with the weight that point is owed, and rejects children that
         * hold no real data there.
         *
         * @param[in]  image  The child to sample
         * @param[out] value  The value sampled from `image`
         * @param[out] weight The weight `value` is owed
         * @param[in]  x      The "x-like" coordinate of the pixel of interest
         * @param[in]  y      The "y-like" coordinate of the pixel of interest
         * @param[in]  band   The band of the pixel to access
         *
         * @return false if `image` is more than a pixel from (x, y), or is
         * transparent there, either of which means it does not share this
         * seam.
         */
        bool get_seam_sample(const ImageData &image,
                             double &value,
                             double &weight,
                             double x,
                             double y,
                             int band) const;
    };

    /**
     * @brief A class to composite heightmap-style ImageData objects together.
     *
     * Heightmap-style ImageData objects have three bands:
     *
     *  0: Real, uninterpolated height values, or 3.4e38 if no data is present.
     *  1: Interpolated height values.
     *  2: Alpha value. 255 if uniterpolated data is present, decaying by
     *     12.5/pixel away from uniterpolated data to a minimum of 1.
     *
     *  If channel 2 is 255, channels 0 and 1 will have the same value.
     *
     * To get a composited value, the images are averaged together and weighted
     * by the value in the alpha channel. An alpha value of 1 is considered to
     * be equivalent to 0.
     *
     */
    class AverageCompositeData final : public CompositeData
    {
    public:
        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };

    /**
     * @brief A class that uses alpha blending to composite heightmap-style
     * ImageData objects together.
     *
     * Heightmap-style ImageData objects have three bands:
     *
     *  0: Real, uninterpolated height values, or 3.4e38 if no data is present.
     *  1: Interpolated height values.
     *  2: Alpha value. 255 if uniterpolated data is present, decaying by
     *     12.5/pixel away from uniterpolated data to a minimum of 1.
     *
     *  If channel 2 is 255, channels 0 and 1 will have the same value.
     *
     */
    class AlphaBlendingCompositeData : public CompositeData
    {
    public:

        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };

    /**
     * @brief A class to composite terrain classification-style ImageData
     * objects.
     *
     * Terrain classification-style ImageData objects have two bands:
     *
     *  0: Integer terrain type.
     *  1: Probability score that the terrain is the given terrain type.
     *
     * To get a composited value, the probability scores for each of the images
     * are compared; the value from the image with the highest score is
     * returned unchanged.
     *
     */
    class ScoredCompositeData final : public CompositeData
    {
    public:
        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };
}

#endif
