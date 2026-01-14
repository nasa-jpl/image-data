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
     * @brief A class that returns the first valid pixel without blending.
     *
     * This composite type is useful for orbital DEMs where tiles have
     * identical edge pixels. It avoids blending different bilinear
     * interpolations of the same location, which can create artificial
     * discontinuities at tile boundaries.
     *
     * Heightmap-style ImageData objects have three bands:
     *
     *  0: Real, uninterpolated height values, or 3.4e38 if no data is present.
     *  1: Interpolated height values.
     *  2: Alpha value. 255 if uniterpolated data is present, decaying by
     *     12.5/pixel away from uniterpolated data to a minimum of 1.
     *
     */
    class FirstValidCompositeData final : public CompositeData
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
