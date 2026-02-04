#include "composite_data.h"

#include <stdexcept>

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
        return total_alpha > minimum_real_alpha;
    }

    bool FirstValidCompositeData::get_pixel_double(double &value,
                                                   const int x,
                                                   const int y,
                                                   const int b) const
    {
        return get_interpolated_pixel_double(
            value, static_cast<double>(x), static_cast<double>(y), b);
    }

    bool FirstValidCompositeData::get_interpolated_pixel_double(
        double &value, const double x, const double y, const int b) const
    {
        // Note: When interpolation is disabled via set_interpolating(false),
        // we still call get_interpolated_pixel_double on child images.
        // The interpolation flag is passed through to children via
        // TranslatedData::set_interpolating, so each child will do
        // nearest-neighbor rounding in its own coordinate space.
        // This is correct because child images need to transform world
        // coordinates to their pixel coordinates before rounding.

        // Simply return the first valid pixel without any blending.
        // This is appropriate for orbital DEMs where tiles have identical
        // edge pixels and blending would create artificial discontinuities.
        for (int i = 0; i < get_count(); i++)
        {
            double current_height = 0.0;

            if (images.at(i)->get_bands() == 1)
            {
                // Support single-band images (PGM format)
                if (images.at(i)->get_interpolated_pixel_double(
                        current_height, x, y, 0))
                {
                    value = current_height;
                    return true;
                }
            }
            else
            {
                // Multi-band images - check alpha for validity
                double current_alpha = 0.0;

                // Check if pixel is in bounds
                if (!images.at(i)->get_interpolated_pixel_double(
                        current_height, x, y, b))
                {
                    continue;
                }

                // Get alpha value
                if (images.at(i)->get_alpha_band() < 0)
                {
                    // No alpha band - assume valid
                    value = current_height;
                    return true;
                }
                else if (!images.at(i)->get_interpolated_pixel_double(
                             current_alpha,
                             x,
                             y,
                             images.at(i)->get_alpha_band()))
                {
                    // Has alpha band but no alpha at this pixel - skip
                    continue;
                }

                // Check if alpha indicates valid data (> 1.01)
                if (current_alpha > 1.01)
                {
                    value = current_height;
                    return true;
                }
            }
        }

        // No valid data found in any image
        return false;
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

    // DistanceWeightedCompositeData implementation

    double DistanceWeightedCompositeData::calculate_distance_scale(
        const std::shared_ptr<ImageData> &img) const
    {
        TerrainBounds bounds = img->get_bounds();
        if (!bounds.valid)
        {
            // Fallback to 10 meters if bounds not available
            return 10.0;
        }

        // Scale proportional to terrain size
        // At distance = scale, weight drops to 50%
        // Use terrain size / 4 so falloff happens within terrain bounds
        double characteristic_size =
            std::max(bounds.get_width(), bounds.get_height());
        return characteristic_size / 4.0;
    }

    void DistanceWeightedCompositeData::add_image_with_origin(
        const std::shared_ptr<ImageData> &img,
        double camera_x,
        double camera_y)
    {
        // Add to base class list
        CompositeData::add_image(img);

        // Calculate and store metadata
        TerrainMetadata metadata;
        metadata.has_camera_origin = true;
        metadata.camera_x = camera_x;
        metadata.camera_y = camera_y;
        metadata.distance_scale = calculate_distance_scale(img);

        terrain_metadata_.push_back(metadata);
    }

    void DistanceWeightedCompositeData::add_image(
        const std::shared_ptr<ImageData> &img, int pos)
    {
        // Add to base class list
        CompositeData::add_image(img, pos);

        // Record metadata without camera origin
        TerrainMetadata metadata;
        metadata.has_camera_origin = false;
        metadata.camera_x = 0.0;
        metadata.camera_y = 0.0;
        metadata.distance_scale = 10.0; // Unused but set for consistency

        terrain_metadata_.push_back(metadata);
    }

    bool DistanceWeightedCompositeData::get_pixel_double(double &value,
                                                         const int x,
                                                         const int y,
                                                         const int b) const
    {
        return get_interpolated_pixel_double(
            value, static_cast<double>(x), static_cast<double>(y), b);
    }

    bool DistanceWeightedCompositeData::get_interpolated_pixel_double(
        double &value, const double x, const double y, const int b) const
    {
        // Note: When interpolation is disabled via set_interpolating(false),
        // we still call get_interpolated_pixel_double on child images.
        // The interpolation flag is passed through to children via
        // TranslatedData::set_interpolating, so each child will do
        // nearest-neighbor rounding in its own coordinate space.
        // This is correct because child images need to transform world
        // coordinates to their pixel coordinates before rounding.

        double weighted_sum = 0.0;
        double total_weight = 0.0;

        for (int i = 0; i < get_count(); i++)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

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
                         current_alpha, x, y, images.at(i)->get_alpha_band()))
            {
                // Valid data value, but no alpha value at this pixel
                // This should not be able to happen
                throw std::runtime_error(
                    "Image pixel at (" + std::to_string(x) + ", " +
                    std::to_string(y) + ") missing alpha band channel");
            }

            // Map alpha value from 1-255 range to 0-1 range
            if (current_alpha < 1.01)
            {
                // If the alpha value is at the minimum, skip this layer
                continue;
            }

            double alpha_weight;
            if (current_alpha > 254.9)
            {
                // If the alpha value is at the maximum, set it to 1.0
                alpha_weight = 1.0;
            }
            else
            {
                // Scale alpha to range 0.0 - 1.0
                alpha_weight = static_cast<int>(current_alpha + 0.5) / 255.0;
            }

            // Calculate combined weight (alpha * distance)
            double combined_weight;
            if (terrain_metadata_[i].has_camera_origin)
            {
                // Calculate distance-based weight
                double dx = x - terrain_metadata_[i].camera_x;
                double dy = y - terrain_metadata_[i].camera_y;
                double dist_sq = dx * dx + dy * dy;

                double scale = terrain_metadata_[i].distance_scale;
                double scale_sq = scale * scale;

                // Quadratic falloff: weight = scale² / (dist² + scale²)
                // At dist=0: weight=1.0
                // At dist=scale: weight=0.5
                // At dist=2*scale: weight=0.2
                double dist_weight = scale_sq / (dist_sq + scale_sq);

                combined_weight = alpha_weight * dist_weight;
            }
            else
            {
                // No camera origin (e.g., orbital DEM) - use only alpha
                // weighting
                combined_weight = alpha_weight;
            }

            weighted_sum += current_height * combined_weight;
            total_weight += combined_weight;
        }

        // If the sum of all the weights is tiny, assume that there's pretty
        // much no data here
        if (total_weight < 0.00001)
        {
            return false;
        }

        value = weighted_sum / total_weight;
        return true;
    }

}
