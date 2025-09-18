#ifndef ZOffsetData_H_
#define ZOffsetData_H_

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

        // Return an exact pixel value as a double
        bool
        get_pixel_double(double &value, int x, int y, int band) const override;

        // Return an interpolated pixel value as a double
        bool get_interpolated_pixel_double(double &value,
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
         * @brief Set the offset and scale for a band of the stored image.
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
    };
}

#endif
