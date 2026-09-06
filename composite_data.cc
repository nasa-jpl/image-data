#include "composite_data.h"

#include <stdexcept>

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>


namespace rsvp
{

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
            return false;
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

        double summed_weight = 0.0;
        double summed_weighted_height = 0.0;

        for (int i = 0; i < get_count(); i++)
        {
            const auto &image = images.at(i);

            double current_height = 0.0;
            double current_alpha = 0.0;

            if (image->get_bands() == 1)
            {
                // Usually we composite `VicarData` images, which have three
                // bands (raw, interpolated, alpha), but we also want to
                // support using `PGMData` images (one raw band). Switch the
                // user-requested band for band 0.
                if (!image->get_interpolated_pixel_double(
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
                // Check the image bounds
                if (!image->get_interpolated_pixel_double(
                        current_height, x, y, b))
                {
                    // Coordinates are out of bounds of image data, so skip
                    // this image
                    continue;
                }

                // Get the alpha value
                if (image->get_alpha_band() < 0)
                {
                    // No alpha band defined for image, so just call it opaque.
                    current_alpha = 255.0;
                }
                else if (!image->get_interpolated_pixel_double(
                             current_alpha, x, y, image->get_alpha_band()))
                {
                    // Valid data value, but no alpha value at this pixel
                    // This should not be able to happen
                    throw std::runtime_error(
                        "Image pixel at (" + std::to_string(x) + ", " +
                        std::to_string(y) + ") missing alpha band channel");
                }
            }

            if (current_alpha < 1.01)
            {
                // An alpha value at the minimum means no data; skip this layer
                continue;
            }

            // Alpha confidence: continuous from 0.0 (alpha 1) to 1.0 (alpha
            // 255). A continuous mapping is essential - the alpha band is
            // bilinearly interpolated, so any step in this function would
            // show up as a step in the composited surface.
            const double normalized_alpha =
                std::min(1.0, (current_alpha - 1.0) / 254.0);
            const double confidence =
                std::pow(normalized_alpha, kAlphaConfidenceExponent);

            // Border feather: ramp the weight down to zero at the child's
            // image rectangle, where the alpha band cannot warn us that the
            // data is about to end.
            double feather = 1.0;
            if (feather_width > 0.0)
            {
                feather = std::min(
                    1.0, image->get_edge_distance(x, y) / feather_width);
            }

            // Floor the weight so that a child with data never contributes
            // exactly zero; a point covered by a single child then returns
            // that child's value even on the child's border.
            const double weight = std::max(confidence * feather, 1e-12);

            summed_weight += weight;
            summed_weighted_height += weight * current_height;
        }

        if (summed_weight <= 0.0)
        {
            // No child had data here
            return false;
        }

        value = summed_weighted_height / summed_weight;
        return true;
    }

    void AlphaBlendingCompositeData::set_feather_width(const double pixels)
    {
        feather_width = pixels;
    }

    double AlphaBlendingCompositeData::get_feather_width() const
    {
        return feather_width;
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

            if (current_score > max_score)
            {
                max_score = current_score;
                value = current_value;
            }
        }

        return value > std::numeric_limits<double>::min();
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

    double CompositeData::get_edge_distance(const double x,
                                            const double y) const
    {
        double distance = 0.0;

        for (const auto &image : images)
        {
            if (image)
            {
                distance = std::max(distance, image->get_edge_distance(x, y));
            }
        }

        return distance;
    }

}
