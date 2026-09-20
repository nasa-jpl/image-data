#ifndef RSVP_IMAGE_DATA_Z_OFFSET_DATA_H
#define RSVP_IMAGE_DATA_Z_OFFSET_DATA_H

#include "image_data.h"


namespace rsvp
{

    /**
     * @brief A class to offset and scale the values of an ImageData.
     *
     * Each band of the internal ImageData have different offsets and scales
     * values. The order of operations scaling and then offsetting:
     *
     *      offset_value = offset + scale * raw_value
     */
    class ZOffsetData final : public ImageData
    {
    private:
        const std::shared_ptr<ImageData> img;

        std::vector<double> offsets;
        std::vector<double> scales;

        /**
         * @brief Whether `band` has an offset and scale of its own.
         *
         * Checked against the tables rather than the stored image's band
         * count, which would cost a walk down the wrappers on every pixel.
         * The tables are sized to that count when this is constructed, and
         * grow when a later band is given an offset and scale.
         */
        bool has_band(int band) const
        {
            return band >= 0 && static_cast<size_t>(band) < scales.size();
        }

        /**
         * @brief Whether `band` is one the stored image has.
         *
         * A band with an offset and scale of its own is known to be, without
         * asking. Only a band without one - a band the stored image gained
         * after the tables were sized, or one it never had - costs the walk
         * down to it to find out.
         */
        bool band_in_range(int band) const
        {
            return has_band(band) || (band >= 0 && band < get_bands());
        }

        /**
         * @brief Offset and scale one raw value of `band`.
         *
         * A band without an offset and scale of its own passes through
         * unchanged.
         */
        double transform(const double raw, const int band) const
        {
            return has_band(band) ? (raw * scales[band]) + offsets[band] : raw;
        }

    public:
        /**
         * @brief Construct a ZOffsetData with identity offset and scaling.
         *
         * The stored ImageData is deleted upon this ZOffsetData's
         * destruction.
         *
         * @param inImg A shared pointer to the ImageData to transform.
         */
        ZOffsetData(const std::shared_ptr<ImageData>& inImg);

        ~ZOffsetData();

        /**
         * @brief Set the image band associated with the alpha blending
         * value.
         *
         * For ZOffsetData, this field is set locally but it is also
         * passed through to the transformed image if it is set.
         *
         * @param[in] band The alpha image band
         */
        void set_alpha_band(int band) override;

        /**
         * @brief Get the image band associated with the alpha blending value.
         *
         * For ZOffsetData, the alpha band of the transformed image is
         * returned if available.
         *
         * @return The alpha image band
         */
        int get_alpha_band() const override;

        /**
         * @brief Enable or disable interpolation for the image data.
         *
         * For ZOffsetData, this field is set locally but is also passed
         * through to the stored image, which is the one that interpolates.
         *
         * @param[in] enable Whether to enable or disable interpolation
         */
        void set_interpolating(bool enable) override;

        /**
         * @brief Get whether or not interpolation is enabled for the image
         * data.
         *
         * For ZOffsetData, the interpolation state of the stored image is
         * returned if available.
         *
         * @return True if interpolation is enabled, false otherwise.
         */
        int get_interpolating() const override;

        // Return an exact pixel value as a double
        bool
        get_pixel_double(double &value, int x, int y, int band) const override;

        // Return an interpolated pixel value as a double
        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override;

        /**
         * @brief Sample the stored image clamped to its nearest valid pixel,
         * and offset and scale the result.
         *
         * The default implementation clamps against this object's own
         * `get_width` and `get_height`, which a ZOffsetData reports on behalf
         * of the stored image. That is only the same coordinate space if the
         * stored image has a pixel grid of its own; wrap a TranslatedData or a
         * CompositeData and it is not. Hand the request down instead, so
         * whichever image does own a grid is the one that clamps to it.
         *
         * @see ImageData::get_clamped_pixel_double
         */
        bool get_clamped_pixel_double(double &value,
                                      double &weight,
                                      double x,
                                      double y,
                                      int band) const override;

        // Return an exact pixel value as an int
        bool get_pixel_int(int &value, int x, int y, int band) const override;

        // Return an interpolated pixel value as an int
        bool get_interpolated_pixel_int(int &value,
                                        double x,
                                        double y,
                                        int band) const override;

        /**
         * @brief Sample several bands of the stored image at once, and offset
         * and scale each.
         *
         * @see ImageData::get_interpolated_bands_double
         */
        bool get_interpolated_bands_double(double *values,
                                           const int *bands,
                                           int count,
                                           double x,
                                           double y) const override;

        /**
         * @brief Set the offset and scale for a band of the stored image.
         *
         * A negative band is ignored.
         *
         * @param band      The band of the image to transform
         * @param offset    The offset to apply to the band's data
         * @param scale     The scale to apply to the band's data
         */
        void set_offset_and_scale(int band, double offset, double scale);

        int get_bands() const override;

        int get_height() const override
        {
            return img ? img->get_height() : 0;
        }

        int get_width() const override
        {
            return img ? img->get_width() : 0;
        }

        /**
         * @brief Where the stored image sits. Offsetting and scaling values
         * does not move pixels, so the answer is the stored image's own.
         */
        TerrainBounds get_bounds() const override
        {
            return img ? img->get_bounds() : TerrainBounds();
        }
    };
}

#endif
