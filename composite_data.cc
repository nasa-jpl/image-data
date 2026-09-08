#include "composite_data.h"

#include <stdexcept>

#include <limits>
#include <list>


namespace rsvp
{

    namespace
    {
        // The weights of the images surrounding a seam sum to exactly 1: that
        // is what makes blending them equal to interpolating over the images
        // as one grid. Anything less means the point is not enclosed by them -
        // it is off the outer edge of the composite, or over a missing image -
        // and filling it in would be extrapolating, not interpolating. The
        // slack absorbs the rounding in the images' placements.
        const double minimum_enclosing_weight = 1.0 - 1e-6;
    }

    CompositeData::CompositeData() = default;

    CompositeData::~CompositeData()
    {
        while (get_count() != 0)
        {
            delete_image(0);
        }
    }

    int CompositeData::get_bands() const
    {
        if (images.empty())
        {
            return 0;
        }
        else
        {
            return images.at(0)->get_bands();
        }
    }

    int CompositeData::get_alpha_band() const
    {
        if (images.empty())
        {
            // Just return the default if there are no children
            return ImageData::get_alpha_band();
        }
        else
        {
            // Return the first child's alpha band
            return images.at(0)->get_alpha_band();
        }
    }

    void CompositeData::add_image(const std::shared_ptr<rsvp::ImageData> &img,
                                  int position)
    {
        // Do not add a nullptr item to our list
        if (img == nullptr)
        {
            return;
        }

        // If we're out of bounds, put the new image at the back
        if (position < 0 || position >= static_cast<int>(images.size()))
        {
            images.push_back(img);
        }
        else
        {
            images.insert(images.begin() + position, img);
        }
    }

    std::shared_ptr<rsvp::ImageData> CompositeData::remove_image(int position)
    {
        if (position >= 0 && position < static_cast<int>(images.size()))
        {
            auto removed_image =
                std::shared_ptr<rsvp::ImageData>(*(images.begin() + position));

            // Update the list
            images.erase(images.begin() + position);

            return removed_image;
        }
        else
        {
            return nullptr;
        }
    }

    bool CompositeData::delete_image(int position)
    {
        if (position >= 0 && position < static_cast<int>(images.size()))
        {
            // Update the list
            images.erase(images.begin() + position);
            return true;
        }
        else
        {
            return false;
        }
    }

    int CompositeData::get_count() const
    {
        return images.size();
    }

    bool AverageCompositeData::get_interpolated_pixel_double(double &value,
                                                             const double x,
                                                             const double y,
                                                             const int b) const
    {
        // Note: When interpolation is disabled via set_interpolating(false),
        // we still call get_interpolated_pixel_double on child images.
        // The interpolation flag is passed through to children via
        // TranslatedData::set_interpolating, so each child will do
        // nearest-neighbor rounding in its own coordinate space.
        // This is correct because child images need to transform world
        // coordinates to their pixel coordinates before rounding.

        double weighted_sum = 0.0;
        double summed_alpha = 0.0;

        for (int i = 0; i < get_count(); i++)
        {
            double height = 0.0;
            double alpha = 0.0;

            if (!images.at(i)->get_interpolated_pixel_double(
                    height, x, y, b) ||
                !images.at(i)->get_interpolated_pixel_double(alpha, x, y, 2))
            {
                // If this image doesn't have a value or an alpha value at this
                // point, skip it
                continue;
            }

            // `1` is the minimum value - snap that to zero.
            if (alpha < 1.01)
            {
                alpha = 0;
            }

            alpha = static_cast<int>(alpha + 0.5) /
                255.0; // Scale alpha to range 0.0 - 1.0

            weighted_sum += height * alpha;
            summed_alpha += alpha;
        }

        // If the sum of all the alphas is tiny, assume that there's pretty
        // much no data here
        if (summed_alpha < 0.00001)
        {
            // ...unless (x, y) lands in the band between abutting images,
            // which none of them can interpolate on its own.
            return get_seam_pixel_double(value, x, y, b);
        }
        else
        {
            value = weighted_sum / summed_alpha;
            return true;
        }
    }

    bool AverageCompositeData::get_pixel_double(double &value,
                                                const int x,
                                                const int y,
                                                const int b) const
    {
        return get_interpolated_pixel_double(
            value, static_cast<double>(x), static_cast<double>(y), b);
    }

    bool AlphaBlendingCompositeData::get_pixel_double(double &value,
                                                      const int x,
                                                      const int y,
                                                      const int b) const
    {
        return get_interpolated_pixel_double(
            value, static_cast<double>(x), static_cast<double>(y), b);
    }

    bool AlphaBlendingCompositeData::get_interpolated_pixel_double(
        double &value, const double x, const double y, const int b) const
    {
        // Note: When interpolation is disabled via set_interpolating(false),
        // we still call get_interpolated_pixel_double on child images.
        // The interpolation flag is passed through to children via
        // TranslatedData::set_interpolating, so each child will do
        // nearest-neighbor rounding in its own coordinate space.
        // This is correct because child images need to transform world
        // coordinates to their pixel coordinates before rounding.

        double total_alpha = 0.0;
        value = 0.0;

        for (int i = 0; i < get_count(); i++)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

            if (images.at(i)->get_bands() == 1)
            {
                // Usually we composite `VicarData` images, which have three
                // bands (raw, interpolated, alpha), but we also want to
                // support using `PGMData` images (one raw band). Switch the
                // user-requested band for band 0.
                if (!images.at(i)->get_interpolated_pixel_double(
                        current_height, x, y, 0))
                {
                    continue;
                }

                // PGMs have no alpha channel, so fake that they are all
                // opaque.
                current_alpha = 255.0;
            }
            else
            {
                // Otherwise, we're not doing the `PGMData` hackery, and should
                // do the expected thing.

                // Check the image bounds
                if (!images.at(i)->get_interpolated_pixel_double(
                        current_height, x, y, b))
                {
                    // Coordinates are out of bounds of image data, so skip
                    // this image
                    continue;
                }

                // Get the alpha value
                if (images.at(i)->get_alpha_band() < 0)
                {
                    // No alpha band defined for image, so just call it opaque.
                    current_alpha = 255.0;
                }
                else if (!images.at(i)->get_interpolated_pixel_double(
                             current_alpha,
                             x,
                             y,
                             images.at(i)->get_alpha_band()))
                {
                    // Valid data value, but no alpha value at this pixel
                    // This should not be able to happen
                    throw std::runtime_error(
                        "Image pixel at (" + std::to_string(x) + ", " +
                        std::to_string(y) + ") missing alpha band channel");
                }
            }

            // Remap the alpha value from the 1-255 range from the image into a
            // 0-1 range.
            if (current_alpha < 1.01)
            {
                // If the alpha value is at the minimum, skip this layer
                continue;
            }
            else if (current_alpha > 254.9)
            {
                // If the alpha value is at the maximum, set it to 1.0
                current_alpha = 1.0;
            }
            else
            {
                // Otherwise, scale alpha between 0 and 1, and then divide it
                // by 1000. This division will have no effect if all layers are
                // extrapolated, but it will heavily weight older opaque data
                // over newer transparent data.
                current_alpha = static_cast<int>(current_alpha + 0.5) /
                    255.0; // Scale alpha to range 0.0 - 1.0
                current_alpha = current_alpha / 1000.0;
            }

            value =
                (current_height *
                     current_alpha + // Contribution from current layer
                 value * total_alpha *
                     (1.0 -
                      current_alpha) // Contribution from all previous layers
                 ) /
                (total_alpha * (1.0 - current_alpha) +
                 current_alpha // New overall alpha value
                );

            // Assign new overall alpha value
            total_alpha = total_alpha * (1.0 - current_alpha) + current_alpha;
        }

        // If the sum of all the alphas is tiny, assume that there's pretty
        // much no data here. The minimum real alpha value is 2/(255 * 1000).
        double minimum_real_alpha = 1.5 / 255.0 / 1000.0;
        if (total_alpha > minimum_real_alpha)
        {
            return true;
        }

        // No image covers (x, y). It may still land in the band between
        // abutting images, which none of them can interpolate on its own.
        return get_seam_pixel_double(value, x, y, b);
    }

    bool ScoredCompositeData::get_pixel_double(double &value,
                                               const int x,
                                               const int y,
                                               const int b) const
    {
        return get_interpolated_pixel_double(value, x, y, b);
    }

    bool ScoredCompositeData::get_interpolated_pixel_double(double &value,
                                                            const double x,
                                                            const double y,
                                                            const int b) const
    {
        // Note: When interpolation is disabled via set_interpolating(false),
        // we still call get_interpolated_pixel_double on child images.
        // The interpolation flag is passed through to children via
        // TranslatedData::set_interpolating, so each child will do
        // nearest-neighbor rounding in its own coordinate space.
        // This is correct because child images need to transform world
        // coordinates to their pixel coordinates before rounding.

        double max_score = std::numeric_limits<double>::min();
        bool covered = false;

        for (int i = 0; i < get_count(); i++)
        {
            double current_score = 0.0;
            double current_value = 0.0;

            if (!images.at(i)->get_interpolated_pixel_double(
                    current_score, x, y, images.at(i)->get_alpha_band()) ||
                !images.at(i)->get_interpolated_pixel_double(
                    current_value, x, y, b))
            {
                // If this image doesn't have a value or an alpha value at this
                // point, skip it
                continue;
            }

            covered = true;

            if (current_score > max_score)
            {
                max_score = current_score;
                value = current_value;
            }
        }

        if (covered)
        {
            return value > std::numeric_limits<double>::min();
        }

        // No image covers (x, y). It may still land in the band between
        // abutting images, which none of them can interpolate on its own. Take
        // the value from the nearest image rather than blending across the
        // seam like the height composites do - these bands hold terrain type
        // identifiers, and the average of two identifiers is a third,
        // unrelated one.
        double max_weight = 0.0;
        double summed_weight = 0.0;
        double nearest_value = 0.0;

        for (int i = 0; i < get_count(); i++)
        {
            double current_value = 0.0;
            double current_weight = 0.0;

            if (!images.at(i)->get_clamped_pixel_double(
                    current_value, current_weight, x, y, b))
            {
                continue;
            }

            summed_weight += current_weight;

            if (current_weight > max_weight)
            {
                max_weight = current_weight;
                nearest_value = current_value;
            }
        }

        // As in get_seam_pixel_double, only a point the images enclose is on a
        // seam
        if (summed_weight < minimum_enclosing_weight)
        {
            return false;
        }

        value = nearest_value;
        return true;
    }

    TerrainBounds CompositeData::get_bounds() const
    {
        TerrainBounds combined_bounds;

        for (const auto &image : images)
        {
            if (image)
            {
                TerrainBounds image_bounds = image->get_bounds();
                combined_bounds.merge(image_bounds);
            }
        }

        return combined_bounds;
    }

    bool CompositeData::get_seam_pixel_double(double &value,
                                              const double x,
                                              const double y,
                                              const int band) const
    {
        double weighted_sum = 0.0;
        double summed_weight = 0.0;

        for (const auto &image : images)
        {
            double current_value = 0.0;
            double current_weight = 0.0;

            if (!image->get_clamped_pixel_double(
                    current_value, current_weight, x, y, band))
            {
                // More than a pixel away from this image, so it is not one of
                // the images sharing this seam.
                continue;
            }

            if (image->get_bands() > 1 && image->get_alpha_band() >= 0)
            {
                double current_alpha = 0.0;
                double alpha_weight = 0.0;

                if (!image->get_clamped_pixel_double(current_alpha,
                                                     alpha_weight,
                                                     x,
                                                     y,
                                                     image->get_alpha_band()))
                {
                    continue;
                }

                if (current_alpha < 1.01)
                {
                    // The minimum alpha value means there is no real data here
                    continue;
                }
            }

            weighted_sum += current_weight * current_value;
            summed_weight += current_weight;
        }

        if (summed_weight < minimum_enclosing_weight)
        {
            return false;
        }

        value = weighted_sum / summed_weight;
        return true;
    }

    bool CompositeData::get_clamped_pixel_double(double &value,
                                                 double &weight,
                                                 const double x,
                                                 const double y,
                                                 const int band) const
    {
        // A composite has no pixel grid of its own to clamp to. If it covers
        // (x, y) at all then nothing needs clamping and it gets the full say.
        if (get_interpolated_pixel_double(value, x, y, band))
        {
            weight = 1.0;
            return true;
        }

        // Otherwise (x, y) is off the composite's outer edge, which is the
        // edge of whichever child lies nearest to it.
        double largest_weight = 0.0;

        for (const auto &image : images)
        {
            double current_value = 0.0;
            double current_weight = 0.0;

            if (!image->get_clamped_pixel_double(
                    current_value, current_weight, x, y, band))
            {
                continue;
            }

            if (current_weight > largest_weight)
            {
                largest_weight = current_weight;
                value = current_value;
            }
        }

        if (largest_weight <= 0.0)
        {
            return false;
        }

        weight = largest_weight;
        return true;
    }

    void CompositeData::set_interpolating(bool enable)
    {
        // Set interpolation flag on this composite
        ImageData::set_interpolating(enable);

        // Pass through to all child images
        for (const auto &image : images)
        {
            if (image)
            {
                image->set_interpolating(enable);
            }
        }
    }

}
