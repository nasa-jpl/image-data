#include "translated_data.h"

#include <cmath>
#include <list>
#include <utility>


namespace rsvp
{

    TranslatedData::TranslatedData(std::shared_ptr<rsvp::ImageData> inImg) :
        t_x(0.0),
        t_y(0.0),
        txx(0.0),
        tyx(0.0),
        txy(0.0),
        tyy(0.0),
        i_x(0.0),
        i_y(0.0),
        ixx(0.0),
        iyx(0.0),
        ixy(0.0),
        iyy(0.0),
        transformed_image(std::move(inImg))
    {

        // Set a default translation - no offset, no scaling, no rotation
        set_trans(0, 0, 1, 0);
    }

    TranslatedData::TranslatedData(std::shared_ptr<rsvp::ImageData> inImg,
                                   double x_offset,
                                   double y_offset,
                                   double scale,
                                   double rotation) :
        t_x(0.0),
        t_y(0.0),
        txx(0.0),
        tyx(0.0),
        txy(0.0),
        tyy(0.0),
        i_x(0.0),
        i_y(0.0),
        ixx(0.0),
        iyx(0.0),
        ixy(0.0),
        iyy(0.0),
        transformed_image(std::move(inImg))
    {

        // Set a default translation - no offset, no scaling, no rotation
        set_trans(x_offset, y_offset, scale, rotation);
    }

    TranslatedData::TranslatedData(std::shared_ptr<rsvp::ImageData> inImg,
                                   double l_x,
                                   double l_y,
                                   double lxx,
                                   double lyx,
                                   double lxy,
                                   double lyy) :
        t_x(0.0),
        t_y(0.0),
        txx(0.0),
        tyx(0.0),
        txy(0.0),
        tyy(0.0),
        i_x(0.0),
        i_y(0.0),
        ixx(0.0),
        iyx(0.0),
        ixy(0.0),
        iyy(0.0),
        transformed_image(std::move(inImg))
    {
        set_trans(l_x, l_y, lxx, lyx, lxy, lyy);
    }

    void TranslatedData::set_trans(double x_offset,
                                   double y_offset,
                                   double scale,
                                   double rotation)
    {
        set_trans(x_offset,
                  y_offset,
                  scale * cos(rotation),
                  -1.0 * scale * sin(rotation),
                  scale * sin(rotation),
                  scale * cos(rotation));
    }

    int TranslatedData::get_bands() const
    {
        if (transformed_image != nullptr)
        {
            return transformed_image->get_bands();
        }
        else
        {
            return 0;
        }
    }

    void TranslatedData::set_trans(
        double l_x, double l_y, double lxx, double lyx, double lxy, double lyy)
    {
        // Affine transformation matrix:
        //     [ X ]   [ txx tyx t_x ]   [ sample ]
        //     [ Y ] = [ txy tyy t_y ] * [  line  ]
        //     [ 1 ]   [  0   0   1  ]   [   1    ]

        t_x = l_x;
        t_y = l_y;
        txx = lxx;
        tyx = lyx;
        txy = lxy;
        tyy = lyy;

        // For a 3x3 affine transformation matrix A with 2x2 rotation component
        // R and 2x1 transformation component t...
        //
        //          [ R | t ]
        //      A = [ - - - ]
        //          [ 0 | 1 ]
        //
        // ... the inverse matrix A' can be computed as a combination of R' and
        // t:
        //
        //           [ R' | -R'*t ]
        //      A' = [ -- - ----- ]
        //           [ 0  |   1   ]
        //
        // If a 2x2 matrix (like R) is invertible, the inverse can be found as:
        //
        //      R = [ a b ]
        //          [ c d ]
        //
        //      R' = (1/ad-bc) * [  d -b ]
        //                       [ -c  a ]

        // Inverse affine transformation matrix:
        //     [ sample ]   [ ixx iyx i_x ]   [ X ]
        //     [  line  ] = [ ixy iyy i_y ] * [ Y ]
        //     [   1    ]   [  0   0   1  ]   [ 1 ]

        // This image just moved, so anything cached about where images sit is
        // out of date
        invalidate_geometry();

        double determinant = txx * tyy - tyx * txy;

        // Compute R'
        ixx = tyy / determinant;
        iyx = -tyx / determinant;
        ixy = -txy / determinant;
        iyy = txx / determinant;

        // Compute -R'*t
        i_x = -ixx * t_x - iyx * t_y;
        i_y = -ixy * t_x - iyy * t_y;

        // Done!
    }

    // Return an exact pixel value as a double
    bool TranslatedData::get_pixel_double(double &value,
                                          const int x,
                                          const int y,
                                          const int band) const
    {
        return get_interpolated_pixel_double(
            value, static_cast<double>(x), static_cast<double>(y), band);
    }

    // Return an interpolated pixel value as a double
    bool TranslatedData::get_interpolated_pixel_double(double &value,
                                                       const double x,
                                                       const double y,
                                                       const int band) const
    {
        if (transformed_image != nullptr)
        {
            double sample = ixx * x + iyx * y + i_x;
            double line = ixy * x + iyy * y + i_y;

            return transformed_image->get_interpolated_pixel_double(
                value, sample, line, band);
        }
        else
        {
            return false;
        }
    }

    bool TranslatedData::get_clamped_pixel_double(double &value,
                                                  double &weight,
                                                  const double x,
                                                  const double y,
                                                  const int band) const
    {
        if (transformed_image == nullptr)
        {
            return false;
        }

        // Clamping has to happen in the transformed image's pixel space, not
        // in ours, so hand the transformed coordinates down rather than using
        // the default implementation.
        const double sample = ixx * x + iyx * y + i_x;
        const double line = ixy * x + iyy * y + i_y;

        return transformed_image->get_clamped_pixel_double(
            value, weight, sample, line, band);
    }

    void TranslatedData::set_alpha_band(int band)
    {
        ImageData::set_alpha_band(band);
        if (transformed_image != nullptr)
        {
            transformed_image->set_alpha_band(band);
        }
    }

    int TranslatedData::get_alpha_band() const
    {
        if (transformed_image != nullptr)
        {
            return transformed_image->get_alpha_band();
        }
        else
        {
            return ImageData::get_alpha_band();
        }
    }

    void TranslatedData::set_interpolating(bool enable)
    {
        ImageData::set_interpolating(enable);
        if (transformed_image != nullptr)
        {
            transformed_image->set_interpolating(enable);
        }
    }

    int TranslatedData::get_interpolating() const
    {
        if (transformed_image != nullptr)
        {
            return transformed_image->get_interpolating();
        }
        else
        {
            return ImageData::get_interpolating();
        }
    }

    TerrainBounds TranslatedData::get_bounds() const
    {
        if (!transformed_image)
        {
            return TerrainBounds();
        }

        TerrainBounds underlying_bounds = transformed_image->get_bounds();

        // An image with a pixel grid does not need to know where it is for us
        // to know where it is: our transform is what places its grid. Only
        // when there is no grid to place do we have to fall back on what it
        // says about itself.
        //
        // This matters for the wedge heightmaps a `ModData` mosaic is built
        // from, which carry their placement as bare labels rather than in the
        // projection group `VicarData` reads, and so report no bounds of their
        // own. Deriving ours from the transform instead keeps the mosaic from
        // being a composite whose children are all in unknown places.
        const bool has_own_grid = get_width() >= 1 && get_height() >= 1;

        if (!has_own_grid && !underlying_bounds.valid)
        {
            return underlying_bounds;
        }

        // The corners of the region we transform, in the stored image's own
        // coordinates.
        double first_sample = 0.0;
        double first_line = 0.0;
        double last_sample = 0.0;
        double last_line = 0.0;

        if (has_own_grid)
        {
            // The stored image has a pixel grid, so our extent is where that
            // grid lands. Bounds describe the extent of the pixel centers, so
            // the far corner is the last pixel - (width - 1, height - 1) - not
            // (width, height).
            last_sample = get_width() - 1;
            last_line = get_height() - 1;
        }
        else
        {
            // No grid of its own, so it is a container of images that are
            // already placed - a CompositeData, say. Its bounds are in the
            // coordinates we transform from, so transform those instead.
            first_sample = underlying_bounds.min_x;
            first_line = underlying_bounds.min_y;
            last_sample = underlying_bounds.max_x;
            last_line = underlying_bounds.max_y;
        }

        double corners_x[4];
        double corners_y[4];

        corners_x[0] = txx * first_sample + tyx * first_line + t_x;
        corners_y[0] = txy * first_sample + tyy * first_line + t_y;

        corners_x[1] = txx * last_sample + tyx * first_line + t_x;
        corners_y[1] = txy * last_sample + tyy * first_line + t_y;

        corners_x[2] = txx * first_sample + tyx * last_line + t_x;
        corners_y[2] = txy * first_sample + tyy * last_line + t_y;

        corners_x[3] = txx * last_sample + tyx * last_line + t_x;
        corners_y[3] = txy * last_sample + tyy * last_line + t_y;

        TerrainBounds result;
        result.valid = true;
        result.min_x = corners_x[0];
        result.max_x = corners_x[0];
        result.min_y = corners_y[0];
        result.max_y = corners_y[0];

        for (int i = 1; i < 4; i++)
        {
            result.min_x = std::min(result.min_x, corners_x[i]);
            result.max_x = std::max(result.max_x, corners_x[i]);
            result.min_y = std::min(result.min_y, corners_y[i]);
            result.max_y = std::max(result.max_y, corners_y[i]);
        }

        return result;
    }
}
