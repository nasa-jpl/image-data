#ifndef TranslatedData_H_
#define TranslatedData_H_

#include "image_data.h"


namespace rsvp
{

    /**
     * @brief Apply a two-dimensional affine transform to an ImageData.
     *
     * This class is most commonly used with .mod and .ht files - the .mod
     * files contain a list of .ht heightmap files and the affine transforms
     * used to transform between SITE x/y space and pixel x/y space.
     *
     * The offsets and scales work such that this TranslatedData...
     *
     *      TranslatedData trans(imagedata, x_offset, y_offset, scale);
     *
     * ... will have this expression evaluate as true:
     *
     *      imagedata.get_pixel_double(x, y, 0) ==
     *          trans.get_pixel_double(scale*x + x_offset, scale*y+y_offset,
     * 0);
     */
    class TranslatedData final : public ImageData
    {
    private:
        // Affine transformation matrix:
        //     [ X ]   [ txx tyx t_x ]   [ sample ]
        //     [ Y ] = [ txy tyy t_y ] * [  line  ]
        //     [ 1 ]   [  0   0   1  ]   [   1    ]
        double t_x, t_y, txx, tyx, txy, tyy;


        // Affine transformation matrix:
        //     [ sample ]   [ ixx iyx i_x ]   [ X ]
        //     [  line  ] = [ ixy iyy i_y ] * [ Y ]
        //     [   1    ]   [  0   0   1  ]   [ 1 ]
        double i_x, i_y, ixx, iyx, ixy, iyy;

        const std::shared_ptr<ImageData> transformed_image;

    public:
        /**
         * @brief Create a new TranslatedData with an identity transform.
         *
         * @param inImg The ImageData to transform
         */
        TranslatedData(std::shared_ptr<rsvp::ImageData> inImg);

        /**
         * @brief Create a new TranslatedData with an offset and the same x and
         * y scaling.
         *
         * @param inImg     The ImageData to transform
         * @param x_offset  The offset to apply to the x values
         * @param y_offset  The offset to apply to the y value
         * @param scale     The scaling to apply to both the x and y values
         * @param rotation  The rotation to apply to the data
         */
        TranslatedData(std::shared_ptr<rsvp::ImageData> inImg,
                       double x_offset,
                       double y_offset,
                       double scale,
                       double rotation);

        /**
         * @brief Create a new TranslatedData with the fully-specified affine
         * transform.
         *
         * @param inImg The ImageData to transform
         * @param l_x   The offset to apply to the x values
         * @param l_y   The offset to apply to the y values
         * @param lxx   The scaling to apply to the x values
         * @param lyx   The skew scaling between y and x
         * @param lxy   The skew scaling between x and y
         * @param lyy   The scaling to apply to the y values
         */
        TranslatedData(std::shared_ptr<rsvp::ImageData> inImg,
                       double l_x,
                       double l_y,
                       double lxx,
                       double lyx,
                       double lxy,
                       double lyy);

        /**
         * @brief Assign the transform to apply to the data.
         *
         * @param l_x   The offset to apply to the x values
         * @param l_y   The offset to apply to the y values
         * @param scale The scaling to apply to both the x and y values
         */
        void set_trans(double x_offset,
                       double y_offset,
                       double scale,
                       double rotation);

        /**
         * @brief Assign the full affine transform to apply to the data.
         *
         * @param l_x   The offset to apply to the x values
         * @param l_y   The offset to apply to the y values
         * @param lxx   The scaling to apply to the x values
         * @param lyx   The skew scaling between y and x
         * @param lxy   The skew scaling between x and y
         * @param lyy   The scaling to apply to the y values
         */
        void set_trans(double l_x,
                       double l_y,
                       double lxx,
                       double lyx,
                       double lxy,
                       double lyy);

        bool
        get_pixel_double(double &value, int x, int y, int band) const override;

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override;

        int get_bands() const override;

        /**
         * @brief Set the image band associated with the alpha blending
         * value.
         *
         * For TranslatedData, this field is set locally but it is also
         * passed through to the transformed image if it is set.
         *
         * @param[in] band The alpha image band
         */
        void set_alpha_band(int band) override;

        /**
         * @brief Get the image band associated with the alpha blending value.
         *
         * For TranslatedData, the alpha band of the transformed image is
         * returned if available.
         *
         * @return The alpha image band
         */
        int get_alpha_band() const override;

        /**
         * @brief Enable or disable interpolation for the image data.
         *
         * For TranslatedData, this field is set locally but is also passed
         * through to the transformed image if it is set.
         *
         * @param[in] enable Whether to enable or disable interpolation
         */
        void set_interpolating(bool enable) override;

        /**
         * @brief Get whether or not interpolation is enabled for the image
         * data.
         *
         * For TranslatedData, the interpolation state of the transformed
         * image is returned if available.
         *
         * @return True if interpolation is enabled, false otherwise.
         */
        int get_interpolating() const override;

        int get_width() const override
        {
            return transformed_image ? transformed_image->get_width() : 0;
        }

        int get_height() const override
        {
            return transformed_image ? transformed_image->get_height() : 0;
        }
    };
}

#endif
