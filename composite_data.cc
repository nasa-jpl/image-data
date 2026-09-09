#include "composite_data.h"

#include <stdexcept>

#include <algorithm>
#include <cstddef>
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

        // How far outside a composite's bounds still counts as inside when
        // ruling out (x, y) being on a seam. Bounds are derived from the
        // images' labels and transforms while pixel lookups go through the
        // inverted transforms, so the two can disagree in the last bits, and
        // rejecting a point the pixels do cover would put a hole in the
        // terrain. A micron is many orders of magnitude above that
        // disagreement and many below the pitch of any real terrain, so it
        // cannot hide a seam either.
        const double bounds_margin = 1e-6;
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

        invalidate_geometry();
    }

    std::shared_ptr<rsvp::ImageData> CompositeData::remove_image(int position)
    {
        if (position >= 0 && position < static_cast<int>(images.size()))
        {
            auto removed_image =
                std::shared_ptr<rsvp::ImageData>(*(images.begin() + position));

            // Update the list
            images.erase(images.begin() + position);
            invalidate_geometry();

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
            invalidate_geometry();
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

        const std::vector<ChildGeometry> &children = child_geometry();

        for (int i = 0; i < get_count(); i++)
        {
            double height = 0.0;
            double alpha = 0.0;

            if (children.at(i).bands == 1)
            {
                // Usually we composite `VicarData` images, which have three
                // bands (raw, interpolated, alpha), but we also want to
                // support using `PGMData` images (one raw band). Switch the
                // user-requested band for band 0.
                if (!images.at(i)->get_interpolated_pixel_double(
                        height, x, y, 0))
                {
                    continue;
                }

                // PGMs have no alpha channel, so fake that they are all
                // opaque.
                alpha = 255.0;
            }
            else if (!images.at(i)->get_interpolated_pixel_double(
                         height, x, y, b))
            {
                // Coordinates are out of bounds of image data, so skip this
                // image
                continue;
            }
            else if (const int band_with_alpha = children.at(i).alpha_band;
                     band_with_alpha < 0)
            {
                // Nothing to say how opaque it is, so just call it opaque.
                alpha = 255.0;
            }
            else if (!images.at(i)->get_interpolated_pixel_double(
                         alpha, x, y, band_with_alpha))
            {
                // Valid data value, but no alpha value at this pixel
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

        const std::vector<ChildGeometry> &children = child_geometry();

        for (int i = 0; i < get_count(); i++)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

            if (children.at(i).bands == 1)
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
                const int band_with_alpha = children.at(i).alpha_band;

                if (band_with_alpha < 0)
                {
                    // Nothing to say how opaque it is, so just call it opaque.
                    current_alpha = 255.0;
                }
                else if (!images.at(i)->get_interpolated_pixel_double(
                             current_alpha, x, y, band_with_alpha))
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
        // abutting images, which none of them can interpolate on its own.
        if (!could_be_on_seam(x, y))
        {
            return false;
        }

        // Take the value from the nearest image rather than blending across
        // the seam like the height composites do - these bands hold terrain
        // type identifiers, and the average of two identifiers is a third,
        // unrelated one.
        double max_weight = 0.0;
        double summed_weight = 0.0;
        double nearest_value = 0.0;

        const std::vector<ChildGeometry> &children = child_geometry();

        for (int i = 0; i < get_count(); i++)
        {
            const auto &image = images.at(i);

            // Only the images flanking the seam have a say
            if (!image || !child_may_reach(children.at(i), x, y))
            {
                continue;
            }

            double current_value = 0.0;
            double current_weight = 0.0;

            // No alpha test here, unlike get_seam_pixel_double: a scored
            // composite takes the best-scoring image everywhere else without
            // ruling out low scores, and a seam should look like the rest of
            // the composite rather than follow a stricter rule of its own.
            if (!image->get_clamped_pixel_double(
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

    double CompositeData::get_clamp_reach_of(const ImageData &image,
                                             const TerrainBounds &bounds)
    {
        if (!bounds.valid)
        {
            return 0.0;
        }

        const int width = image.get_width();
        const int height = image.get_height();

        if (width < 2 || height < 2)
        {
            // Nothing to measure a pixel against. An image with no grid of its
            // own is a container of images that are already placed, and how
            // far past its edge it can still reach is a question for whichever
            // of those the point is near.
            return 0.0;
        }

        // A clamped sample reaches one of the image's own pixels past its
        // edge, so the reach we want is that pixel's pitch in our coordinates.
        //
        // Bounds are axis-aligned, so for a rotated image each span covers
        // more ground than the pitch along that axis. Taking the larger of the
        // two estimates is therefore never short of the true pitch, whatever
        // the rotation, and erring long only costs us a cull we could have
        // made.
        return std::max(bounds.get_width() / (width - 1),
                        bounds.get_height() / (height - 1));
    }

    void CompositeData::refresh_geometry_cache() const
    {
        const unsigned long version = geometry_version();

        if (cached_geometry_version == version)
        {
            return;
        }

        TerrainBounds combined_bounds;

        cached_children.clear();
        cached_children.resize(images.size());

        for (size_t i = 0; i < images.size(); i++)
        {
            const auto &image = images.at(i);

            if (!image)
            {
                // Leave the entry at its defaults so the indices still line up
                continue;
            }

            ChildGeometry &info = cached_children.at(i);

            info.bounds = image->get_bounds();
            info.clamp_reach = get_clamp_reach_of(*image, info.bounds);
            info.bands = image->get_bands();
            info.alpha_band = get_alpha_band_of(*image);

            combined_bounds.merge(info.bounds);
        }

        cached_bounds = combined_bounds;
        cached_geometry_version = version;
    }

    const TerrainBounds &CompositeData::merged_bounds() const
    {
        // Only for the side effect of refilling the cache the bounds live in
        child_geometry();

        return cached_bounds;
    }

    bool CompositeData::child_may_reach(const ChildGeometry &info,
                                        const double x,
                                        const double y)
    {
        if (!info.bounds.valid || info.clamp_reach <= 0.0)
        {
            // Nothing known about the child, so nothing ruled out
            return true;
        }

        return info.bounds.contains(x, y, info.clamp_reach + bounds_margin);
    }

    TerrainBounds CompositeData::get_bounds() const
    {
        return merged_bounds();
    }

    bool CompositeData::could_be_on_seam(const double x, const double y) const
    {
        // It takes two images to have a band between them
        if (get_count() < 2)
        {
            return false;
        }

        // Seams run between the images, so a point beyond the outer edge of
        // all of them is not on one. Images that do not know where they are
        // leave the bounds invalid, and then this rules nothing out.
        const TerrainBounds &bounds = merged_bounds();

        return !bounds.valid || bounds.contains(x, y, bounds_margin);
    }

    int CompositeData::get_alpha_band_of(const ImageData &image)
    {
        if (image.get_bands() <= 1)
        {
            // A single-band image - a PGM - carries no alpha at all
            return -1;
        }

        const int declared_band = image.get_alpha_band();

        if (declared_band >= 0)
        {
            return declared_band;
        }

        // The image has not said where its alpha is. `ModData` labels every
        // image it reads, so this only comes up for images assembled by hand,
        // and for those the format is the best guide there is: a three-band
        // heightmap keeps its alpha in band 2.
        return (image.get_bands() > 2) ? 2 : -1;
    }

    bool CompositeData::get_seam_sample(const ImageData &image,
                                        const ChildGeometry &info,
                                        double &value,
                                        double &weight,
                                        const double x,
                                        const double y,
                                        const int band) const
    {
        if (!image.get_clamped_pixel_double(value, weight, x, y, band))
        {
            // More than a pixel away from this image, so it is not one of the
            // images sharing this seam.
            return false;
        }

        const int band_with_alpha = info.alpha_band;

        if (band_with_alpha < 0)
        {
            // Nothing to say whether the data is real, so take it as real
            return true;
        }

        double alpha = 0.0;
        double alpha_weight = 0.0;

        if (!image.get_clamped_pixel_double(
                alpha, alpha_weight, x, y, band_with_alpha))
        {
            return false;
        }

        // The minimum alpha value means there is no real data here
        return alpha >= 1.01;
    }

    bool CompositeData::get_seam_pixel_double(double &value,
                                              const double x,
                                              const double y,
                                              const int band) const
    {
        if (!could_be_on_seam(x, y))
        {
            return false;
        }

        double weighted_sum = 0.0;
        double summed_weight = 0.0;

        const std::vector<ChildGeometry> &children = child_geometry();

        for (size_t i = 0; i < images.size(); i++)
        {
            const auto &image = images.at(i);
            const ChildGeometry &info = children.at(i);

            // Only the one or two images flanking the seam have a say, so rule
            // the rest out before walking down into them
            if (!image || !child_may_reach(info, x, y))
            {
                continue;
            }

            double current_value = 0.0;
            double current_weight = 0.0;

            if (!get_seam_sample(
                    *image, info, current_value, current_weight, x, y, band))
            {
                continue;
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

        const std::vector<ChildGeometry> &children = child_geometry();

        for (size_t i = 0; i < images.size(); i++)
        {
            const auto &image = images.at(i);

            // Only a child within a pixel of (x, y) can be the nearest one
            if (!image || !child_may_reach(children.at(i), x, y))
            {
                continue;
            }

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
