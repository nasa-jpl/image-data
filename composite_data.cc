#include "composite_data.h"

#include <stdexcept>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <string>


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

        bool same_bounds(const TerrainBounds &first,
                         const TerrainBounds &second)
        {
            if (!first.valid || !second.valid)
            {
                // Nothing is known about where an invalid bounds sits, so two
                // of them are only interchangeable if neither is valid
                return first.valid == second.valid;
            }

            return first.min_x == second.min_x &&
                first.max_x == second.max_x && first.min_y == second.min_y &&
                first.max_y == second.max_y &&
                first.pixel_reach == second.pixel_reach;
        }
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

        // Resolved once, so that the seam fallback below answers from the same
        // placement of the children that this loop blended
        const GeometrySnapshot &snapshot = geometry();

        for (size_t i = 0; i < images.size(); i++)
        {
            double height = 0.0;
            double alpha = 0.0;

            if (!sample_child(snapshot, i, x, y, b, height, alpha))
            {
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
            return get_seam_pixel_double(value, snapshot, x, y, b);
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

        // Resolved once, so that the seam fallback below answers from the same
        // placement of the children that this loop blended
        const GeometrySnapshot &snapshot = geometry();

        for (size_t i = 0; i < images.size(); i++)
        {
            double current_height = 0.0;
            double current_alpha = 0.0;

            if (!sample_child(
                    snapshot, i, x, y, b, current_height, current_alpha))
            {
                continue;
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
        return get_seam_pixel_double(value, snapshot, x, y, b);
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

        const GeometrySnapshot &snapshot = geometry();
        const std::vector<ChildGeometry> &children = snapshot.children;

        for (size_t i = 0; i < images.size(); i++)
        {
            const ChildGeometry &info = children[i];

            if (!child_may_reach(info, x, y))
            {
                // Certainly nowhere near (x, y)
                continue;
            }

            const ImageData &image = *images[i];

            // The score and the value, from one walk down the child. The
            // band the child scores by is asked for rather than read from
            // the snapshot: a child whose `set_alpha_band` does not reach
            // `ImageData::set_alpha_band` never invalidates the snapshot,
            // and this must not go on scoring it by a band it has moved
            // away from.
            const int bands[2] = {image.get_alpha_band(), b};
            double sampled[2] = {0.0, 0.0};

            if (!image.get_interpolated_bands_double(sampled, bands, 2, x, y))
            {
                // If this image doesn't have a value or an alpha value at this
                // point, skip it
                continue;
            }

            const double current_score = sampled[0];

            covered = true;

            if (current_score > max_score)
            {
                max_score = current_score;
                value = sampled[1];
            }
        }

        if (covered)
        {
            return value > std::numeric_limits<double>::min();
        }

        // No image covers (x, y). It may still land in the band between
        // abutting images, which none of them can interpolate on its own.
        if (!could_be_on_seam(snapshot, x, y))
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

        for (size_t i = 0; i < images.size(); i++)
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

    bool CompositeData::describes_same_geometry(const GeometrySnapshot &first,
                                                const GeometrySnapshot &second)
    {
        if (first.children.size() != second.children.size() ||
            !same_bounds(first.bounds, second.bounds))
        {
            return false;
        }

        for (size_t i = 0; i < first.children.size(); i++)
        {
            const ChildGeometry &one = first.children.at(i);
            const ChildGeometry &other = second.children.at(i);

            if (one.bands != other.bands ||
                one.alpha_band != other.alpha_band ||
                !same_bounds(one.bounds, other.bounds))
            {
                return false;
            }
        }

        return true;
    }

    const CompositeData::GeometrySnapshot &
    CompositeData::refresh_geometry_cache() const
    {
        // Lookups read a published snapshot without locking, so this only has
        // to keep two threads from working one out at the same time.
        const std::lock_guard<std::mutex> lock(geometry_mutex);

        const GeometrySnapshot *published =
            published_geometry.load(std::memory_order_relaxed);

        const unsigned long version = geometry_version();

        if (published != nullptr &&
            published->version.load(std::memory_order_relaxed) == version)
        {
            // Another thread worked this version out while we waited
            return *published;
        }

        const auto snapshot = std::make_shared<GeometrySnapshot>();

        snapshot->children.resize(images.size());
        snapshot->version.store(version, std::memory_order_relaxed);

        for (size_t i = 0; i < images.size(); i++)
        {
            const auto &image = images.at(i);

            if (!image)
            {
                // Leave the entry at its defaults so the indices still line up
                continue;
            }

            ChildGeometry &info = snapshot->children.at(i);

            info.bounds = image->get_bounds();
            info.bands = image->get_bands();
            info.alpha_band = get_alpha_band_of(*image);

            snapshot->bounds.merge(info.bounds);
            snapshot->all_bounded &= info.bounds.valid;
        }

        if (images.empty())
        {
            // Nothing bounds anything
            snapshot->all_bounded = false;
        }

        if (!snapshot->all_bounded)
        {
            // A child that does not know where it is could answer anywhere,
            // so a parent composite must not skip this one by its bounds
            snapshot->bounds.pixel_reach = 0.0;
        }

        if (published != nullptr &&
            describes_same_geometry(*published, *snapshot))
        {
            // The version counter is global, so most of what invalidates a
            // cache is some other image moving. Restamp the snapshot lookups
            // are already reading rather than retiring it for an identical
            // one, which keeps them off this slow path without adding to what
            // `retained_geometry` has to hold on to.
            published->version.store(version, std::memory_order_relaxed);

            return *published;
        }

        // Own it before pointing lookups at it, and keep owning it afterwards:
        // a lookup that has already loaded the previous snapshot is still
        // reading it, and nothing here can tell when it has stopped.
        retained_geometry.push_back(snapshot);

        published_geometry.store(snapshot.get(), std::memory_order_release);

        return *snapshot;
    }

    bool CompositeData::child_may_reach(const ChildGeometry &info,
                                        const double x,
                                        const double y)
    {
        // Every path that skips a child comes through here, so this is the
        // one place that has to get it right: a child whose bounds or reach
        // are unknown is never ruled out
        return info.bounds.could_reach(x, y, bounds_margin);
    }

    TerrainBounds CompositeData::get_bounds() const
    {
        return geometry().bounds;
    }

    bool CompositeData::sample_child(const GeometrySnapshot &snapshot,
                                     const size_t index,
                                     const double x,
                                     const double y,
                                     const int band,
                                     double &value,
                                     double &alpha) const
    {
        const ChildGeometry &info = snapshot.children[index];

        // Most children of a mosaic are nowhere near any given point, and
        // finding that out by sampling one costs a walk down its wrappers,
        // an inverse transform and a bounds check. Rule it out by its bounds
        // first where that is safe.
        if (!child_may_reach(info, x, y))
        {
            return false;
        }

        const ImageData &image = *images[index];

        if (info.bands == 1)
        {
            // Usually we composite `VicarData` images, which have three
            // bands (raw, interpolated, alpha), but we also want to
            // support using `PGMData` images (one raw band). Switch the
            // user-requested band for band 0.
            if (!image.get_interpolated_pixel_double(value, x, y, 0))
            {
                return false;
            }

            // PGMs have no alpha channel, so fake that they are all opaque.
            alpha = 255.0;
            return true;
        }

        if (info.alpha_band < 0)
        {
            if (!image.get_interpolated_pixel_double(value, x, y, band))
            {
                // Coordinates are out of bounds of image data
                return false;
            }

            // Nothing to say how opaque it is, so just call it opaque.
            alpha = 255.0;
            return true;
        }

        // The value and its alpha from one walk down the child
        const int bands[2] = {band, info.alpha_band};
        double sampled[2] = {0.0, 0.0};

        if (!image.get_interpolated_bands_double(sampled, bands, 2, x, y))
        {
            // Almost always the point is outside the child. A value that
            // reads on its own means it is the alpha band that is missing:
            // the child was labelled with a band it does not have, which is
            // a mistake to report rather than a pixel of no data. Only
            // failed lookups pay for the second walk.
            if (image.get_interpolated_pixel_double(value, x, y, band))
            {
                throw std::runtime_error(
                    "Image pixel at (" + std::to_string(x) + ", " +
                    std::to_string(y) + ") has no alpha band " +
                    std::to_string(info.alpha_band));
            }

            return false;
        }

        value = sampled[0];
        alpha = sampled[1];
        return true;
    }

    bool CompositeData::could_be_on_seam(const GeometrySnapshot &snapshot,
                                         const double x,
                                         const double y) const
    {
        // It takes two images to have a band between them
        if (get_count() < 2)
        {
            return false;
        }

        // Seams run between the images, so a point beyond the outer edge of
        // all of them is not on one. That only follows when the merged bounds
        // are where every child's pixels are: a child that does not know
        // where it is, or one whose bounds are not in the coordinates its
        // lookups take, could have a seam anywhere, and then this rules
        // nothing out.
        return !snapshot.all_bounded ||
            snapshot.bounds.contains(x, y, bounds_margin);
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
                                              const GeometrySnapshot &snapshot,
                                              const double x,
                                              const double y,
                                              const int band) const
    {
        if (!could_be_on_seam(snapshot, x, y))
        {
            return false;
        }

        double weighted_sum = 0.0;
        double summed_weight = 0.0;

        const std::vector<ChildGeometry> &children = snapshot.children;

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

        const std::vector<ChildGeometry> &children = geometry().children;

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
