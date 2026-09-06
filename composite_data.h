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
         * @brief Get the largest edge distance reported by any child image.
         *
         * @return The distance in the child's pixel units.
         */
        double get_edge_distance(double x, double y) const override;
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
     * @brief A class that uses feathered alpha blending to composite
     * heightmap-style ImageData objects together.
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
     * The composited value is a weighted mean of every child that has data at
     * the point (alpha > 1). Each child's weight is the product of:
     *
     *  - an alpha confidence, ((alpha - 1) / 254)^kAlphaConfidenceExponent,
     *    which is continuous, 1.0 for real data and 0.0 at alpha 1. Because
     *    the alpha band already ramps down over ~20 pixels away from real
     *    data, this feathers every interior data boundary; and
     *  - a border feather, min(1, edge_distance / feather_width), which ramps
     *    the weight down to zero at the child's image rectangle (where alpha
     *    gives no warning), so tiles that end inside other tiles blend rather
     *    than cut.
     *
     * The result is independent of the order in which children were added,
     * and a point covered by a single child returns that child's value
     * exactly. Band 0 is only meaningful where alpha is 255 or exactly 1; use
     * band 1 for blended heights.
     */
    class AlphaBlendingCompositeData : public CompositeData
    {
    public:
        /// Exponent applied to the normalized alpha (0..1) to form the
        /// confidence weight. Higher values prefer real data more strongly
        /// over extrapolated data at the cost of steeper transitions.
        static constexpr double kAlphaConfidenceExponent = 2.0;

        /// Default width, in child pixels, over which a child's weight ramps
        /// from zero at its image border to full. Matches the ~20 pixel alpha
        /// ramp used by heightmap-style images.
        static constexpr double kDefaultFeatherWidth = 20.0;

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

        /**
         * @brief Set the border feather width.
         *
         * @param[in] pixels Width of the ramp in child pixels. Values <= 0
         * disable border feathering entirely.
         */
        void set_feather_width(double pixels);

        /**
         * @brief Get the border feather width in child pixels.
         */
        double get_feather_width() const;

    private:
        double feather_width = kDefaultFeatherWidth;
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
