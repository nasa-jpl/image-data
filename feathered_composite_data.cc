#include "feathered_composite_data.h"

#include <algorithm>
#include <cmath>
#include <limits>


namespace rsvp
{

    FeatheredCompositeData::FeatheredCompositeData(double feather_distance) :
        feather_distance_(feather_distance)
    {
    }

    FeatheredCompositeData::~FeatheredCompositeData() = default;

    double FeatheredCompositeData::smoothstep(double t)
    {
        // Clamp t to [0, 1]
        if (t <= 0.0)
        {
            return 0.0;
        }
        if (t >= 1.0)
        {
            return 1.0;
        }

        // Smoothstep formula: 3t^2 - 2t^3 = t^2 * (3 - 2t)
        return t * t * (3.0 - 2.0 * t);
    }

    void FeatheredCompositeData::add_image(
        const std::shared_ptr<rsvp::ImageData> &img,
        bool is_orbital,
        int position)
    {
        // Do not add a nullptr item to our list
        if (img == nullptr)
        {
            return;
        }

        // Add to appropriate list based on orbital status
        if (is_orbital)
        {
            if (position < 0 ||
                position >= static_cast<int>(orbital_images_.size()))
            {
                orbital_images_.push_back(img);
            }
            else
            {
                orbital_images_.insert(orbital_images_.begin() + position, img);
            }
        }
        else
        {
            if (position < 0 ||
                position >= static_cast<int>(non_orbital_images_.size()))
            {
                non_orbital_images_.push_back(img);
            }
            else
            {
                non_orbital_images_.insert(non_orbital_images_.begin() +
                                               position,
                                           img);
            }
        }

        // Also add to the base class images list for compatibility
        CompositeData::add_image(img, position);
    }

    bool FeatheredCompositeData::has_other_non_orbital_data(
        double x,
        double y,
        const std::shared_ptr<rsvp::ImageData> &exclude_image) const
    {
        // Check if any other non-orbital terrain has solid (alpha ~= 255) data
        // at this location
        for (const auto &image : non_orbital_images_)
        {
            if (image == exclude_image)
            {
                continue;
            }

            double alpha = 0.0;
            if (image->get_alpha_band() >= 0 &&
                image->get_interpolated_pixel_double(alpha,
                                                     x,
                                                     y,
                                                     image->get_alpha_band()))
            {
                // If alpha is close to 255, this is solid data
                if (alpha > 254.9)
                {
                    return true;
                }
            }
        }

        return false;
    }

    double FeatheredCompositeData::compute_distance_to_edge(
        const std::shared_ptr<rsvp::ImageData> &image,
        double x,
        double y) const
    {
        // Get alpha value at query point
        double center_alpha = 0.0;
        const int img_alpha_band = image->get_alpha_band();

        if (img_alpha_band < 0)
        {
            // No alpha band, treat as fully opaque
            return -feather_distance_;
        }

        if (!image->get_interpolated_pixel_double(center_alpha, x, y,
                                                  img_alpha_band))
        {
            // No data at this point - treat as if alpha=0 and search for edge
            center_alpha = 0.0;
        }

        // If we're in solid region (alpha ~= 255), we're inside
        if (center_alpha > 254.9)
        {
            // Search outward to find edge
            // Use a sampling resolution based on the image (assume ~0.01m per
            // pixel as typical)
            const double sample_step = 0.1; // 10cm steps
            const double max_search_distance = feather_distance_ + 1.0;

            for (double dist = sample_step; dist < max_search_distance;
                 dist += sample_step)
            {
                // Sample in multiple directions
                const int num_directions = 8;
                for (int dir = 0; dir < num_directions; ++dir)
                {
                    const double angle = 2.0 * M_PI * dir / num_directions;
                    const double test_x = x + dist * std::cos(angle);
                    const double test_y = y + dist * std::sin(angle);

                    double test_alpha = 0.0;
                    if (image->get_interpolated_pixel_double(test_alpha,
                                                             test_x,
                                                             test_y,
                                                             img_alpha_band))
                    {
                        if (test_alpha < 254.9)
                        {
                            // Found edge - return negative distance (inside)
                            return -dist;
                        }
                    }
                    else
                    {
                        // Hit boundary of image data
                        return -dist;
                    }
                }
            }

            // No edge found within search radius, deep inside
            return -max_search_distance;
        }
        else if (center_alpha < 1.01)
        {
            // We're in no-data region, search for nearest solid data
            const double sample_step = 0.1;
            const double max_search_distance = feather_distance_ + 1.0;

            for (double dist = sample_step; dist < max_search_distance;
                 dist += sample_step)
            {
                const int num_directions = 8;
                for (int dir = 0; dir < num_directions; ++dir)
                {
                    const double angle = 2.0 * M_PI * dir / num_directions;
                    const double test_x = x + dist * std::cos(angle);
                    const double test_y = y + dist * std::sin(angle);

                    double test_alpha = 0.0;
                    if (image->get_interpolated_pixel_double(test_alpha,
                                                             test_x,
                                                             test_y,
                                                             img_alpha_band))
                    {
                        if (test_alpha > 254.9)
                        {
                            // Found solid data - return positive distance
                            // (outside)
                            return dist;
                        }
                    }
                }
            }

            // No solid data found within search radius
            return max_search_distance;
        }
        else
        {
            // We're in extrapolated region (1 < alpha < 255)
            // The alpha value decays by 12.5 per pixel in the existing
            // implementation Estimate distance based on alpha falloff
            // Alpha formula: alpha = 255 - 12.5 * distance_in_pixels
            // Assuming 0.01m per pixel: distance = (255 - alpha) / 12.5 * 0.01

            const double pixels_from_edge = (255.0 - center_alpha) / 12.5;
            const double meters_per_pixel = 0.01; // Typical value
            const double distance = pixels_from_edge * meters_per_pixel;

            return distance;
        }
    }

    double FeatheredCompositeData::compute_feathering_weight(double x,
                                                              double y) const
    {
        // Weight of 1.0 means use non-orbital fully
        // Weight of 0.0 means use orbital fully

        double max_weight = 0.0;

        // Check each non-orbital terrain
        for (const auto &image : non_orbital_images_)
        {
            double alpha = 0.0;
            const int img_alpha_band = image->get_alpha_band();

            if (img_alpha_band < 0)
            {
                // No alpha band - treat as fully opaque
                max_weight = 1.0;
                continue;
            }

            // Get alpha value if available
            bool has_alpha_data =
                image->get_interpolated_pixel_double(alpha, x, y, img_alpha_band);

            // Compute distance to edge to determine feathering
            double dist_to_edge = compute_distance_to_edge(image, x, y);

            // Determine if we should process this terrain
            // Process if: we have alpha data, OR we're within feather distance
            if (!has_alpha_data && dist_to_edge >= feather_distance_)
            {
                // No data and too far from edge - skip this terrain
                continue;
            }

            // Check if we're at a hole (another non-orbital terrain has solid
            // data)
            bool at_hole = has_other_non_orbital_data(x, y, image);

            if (at_hole)
            {
                // We're at a hole - use alpha blending, no feathering
                if (has_alpha_data && alpha >= 1.01)
                {
                    double weight = alpha / 255.0;
                    max_weight = std::max(max_weight, weight);
                }
                // If no alpha data at hole, skip
                continue;
            }

            // Not at a hole - apply feathering based on distance from edge
            if (dist_to_edge < 0.0)
            {
                // Inside solid region - use full non-orbital weight
                max_weight = 1.0;
            }
            else if (dist_to_edge < feather_distance_)
            {
                // Within feathering zone - apply sigmoid transition
                double t = dist_to_edge / feather_distance_;
                double weight = 1.0 - smoothstep(t);
                max_weight = std::max(max_weight, weight);
            }
            // else: beyond feathering zone, weight stays at 0 (or previous
            // max)
        }

        return max_weight;
    }

    bool FeatheredCompositeData::blend_non_orbital_terrains(double &value,
                                                             double x,
                                                             double y,
                                                             int band) const
    {
        // Use alpha blending to combine non-orbital terrains
        // This is similar to AlphaBlendingCompositeData logic
        double total_alpha = 0.0;
        value = 0.0;

        for (const auto &image : non_orbital_images_)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

            // Get height value
            if (!image->get_interpolated_pixel_double(current_height, x, y,
                                                      band))
            {
                // No data from this terrain
                continue;
            }

            // Get alpha value
            const int img_alpha_band = image->get_alpha_band();
            if (img_alpha_band < 0)
            {
                // No alpha band, treat as fully opaque
                current_alpha = 255.0;
            }
            else if (!image->get_interpolated_pixel_double(current_alpha,
                                                           x,
                                                           y,
                                                           img_alpha_band))
            {
                // No alpha value - check if we're within feathering distance
                double dist_to_edge = compute_distance_to_edge(image, x, y);
                if (dist_to_edge < feather_distance_)
                {
                    // Within feathering zone - use the height with alpha=0
                    current_alpha = 0.0;
                }
                else
                {
                    continue;
                }
            }

            // Remap alpha value from 1-255 range to 0-1 range
            // Allow alpha=0 within feathering zone
            bool in_feathering_zone = false;
            if (current_alpha < 0.01)
            {
                // Check if within feathering distance
                double dist_to_edge = compute_distance_to_edge(image, x, y);
                if (dist_to_edge >= feather_distance_)
                {
                    continue; // Skip if no data and outside feathering zone
                }
                // Within feathering zone - use height directly, feathering will
                // handle weighting
                value = current_height;
                in_feathering_zone = true;
            }
            else if (current_alpha > 254.9)
            {
                current_alpha = 1.0; // Fully opaque
            }
            else
            {
                // Scale alpha and weight extrapolated data less
                current_alpha = static_cast<int>(current_alpha + 0.5) / 255.0;
                current_alpha = current_alpha / 1000.0;
            }

            if (!in_feathering_zone)
            {
                // Alpha blend (normal case)
                value = (current_height * current_alpha +
                         value * total_alpha * (1.0 - current_alpha)) /
                    (total_alpha * (1.0 - current_alpha) + current_alpha);

                total_alpha = total_alpha * (1.0 - current_alpha) + current_alpha;
            }
        }

        // Check if we have valid data
        // If we have no alpha but we have a value (from feathering zone), return true
        if (total_alpha < 0.0001 && value != 0.0)
        {
            return true; // Have height data from feathering zone
        }

        double minimum_real_alpha = 1.5 / 255.0 / 1000.0;
        return total_alpha > minimum_real_alpha;
    }

    bool FeatheredCompositeData::blend_orbital_terrains(double &value,
                                                         double x,
                                                         double y,
                                                         int band) const
    {
        // Use alpha blending to combine orbital terrains
        double total_alpha = 0.0;
        value = 0.0;

        for (const auto &image : orbital_images_)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

            // Get height value
            if (!image->get_interpolated_pixel_double(current_height, x, y,
                                                      band))
            {
                continue;
            }

            // Get alpha value
            const int img_alpha_band = image->get_alpha_band();
            if (img_alpha_band < 0)
            {
                current_alpha = 255.0;
            }
            else if (!image->get_interpolated_pixel_double(current_alpha,
                                                           x,
                                                           y,
                                                           img_alpha_band))
            {
                continue;
            }

            // Remap alpha
            if (current_alpha < 1.01)
            {
                continue;
            }
            else if (current_alpha > 254.9)
            {
                current_alpha = 1.0;
            }
            else
            {
                current_alpha = static_cast<int>(current_alpha + 0.5) / 255.0;
                current_alpha = current_alpha / 1000.0;
            }

            // Alpha blend
            value = (current_height * current_alpha +
                     value * total_alpha * (1.0 - current_alpha)) /
                (total_alpha * (1.0 - current_alpha) + current_alpha);

            total_alpha = total_alpha * (1.0 - current_alpha) + current_alpha;
        }

        double minimum_real_alpha = 1.5 / 255.0 / 1000.0;
        return total_alpha > minimum_real_alpha;
    }

    bool FeatheredCompositeData::get_interpolated_pixel_double(
        double &value, const double x, const double y, const int band) const
    {
        // Main feathering logic

        // 1. Blend non-orbital terrains
        double non_orbital_height = 0.0;
        bool has_non_orbital =
            blend_non_orbital_terrains(non_orbital_height, x, y, band);

        if (!has_non_orbital)
        {
            // No non-orbital data, use orbital only
            return blend_orbital_terrains(value, x, y, band);
        }

        // 2. Compute feathering weight
        double weight = compute_feathering_weight(x, y);

        // 3. If weight is 1.0, use non-orbital only
        if (weight >= 0.9999)
        {
            value = non_orbital_height;
            return true;
        }

        // 4. If weight is 0.0, use orbital only
        if (weight <= 0.0001)
        {
            return blend_orbital_terrains(value, x, y, band);
        }

        // 5. Blend between non-orbital and orbital based on weight
        double orbital_height = 0.0;
        bool has_orbital = blend_orbital_terrains(orbital_height, x, y, band);

        if (!has_orbital)
        {
            // No orbital terrain exists, fall back to non-orbital only
            value = non_orbital_height;
            return true;
        }

        // Final blend
        value = non_orbital_height * weight + orbital_height * (1.0 - weight);
        return true;
    }

    bool FeatheredCompositeData::get_pixel_double(double &value,
                                                   const int x,
                                                   const int y,
                                                   const int band) const
    {
        return get_interpolated_pixel_double(value,
                                             static_cast<double>(x),
                                             static_cast<double>(y),
                                             band);
    }

} // namespace rsvp
