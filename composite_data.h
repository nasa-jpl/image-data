#ifndef RSVP_IMAGE_DATA_COMPOSITE_DATA_H
#define RSVP_IMAGE_DATA_COMPOSITE_DATA_H

#include "image_data.h"

#include <atomic>
#include <mutex>


namespace rsvp
{

    /**
     * @brief A class to composite multiple ImageData objects together.
     *
     * Pixel lookups are safe to run from several threads at once. Everything
     * else is not: adding, removing or moving a child while a lookup is in
     * flight races with that lookup, both over the list of children and over
     * the child's own transform. Place the children first, then sample.
     *
     * A composite works out where its children sit once and holds on to that
     * answer, and a lookup reads it without locking. That costs a small amount
     * of memory each time the children genuinely change, which is never given
     * back until the composite is destroyed - a lookup on another thread may
     * still be reading the previous answer, and there is no way to tell when
     * it has stopped. Placing children once, as reading a mosaic from a file
     * does, costs one such answer. Moving a child repeatedly - dragging one
     * around, or animating it - accumulates them, so a caller that does that
     * over a long session should expect the memory to creep.
     */
    class CompositeData : public ImageData
    {

    protected:
        /**
         * @brief What a composite needs to know about one of its children but
         * cannot afford to ask for once per pixel.
         *
         * Asking a child where it is walks the chain of wrappers around it,
         * transforming corners on the way back up, and asking which band
         * holds its alpha walks it again. Both answers hold until an image
         * moves or is relabelled.
         */
        struct ChildGeometry
        {
            /**
             * Where the child's pixels are, in this composite's coordinates,
             * and how far past them a lookup on the child still lands on one.
             * Invalid for a child that does not know, and then the child is
             * never skipped.
             */
            TerrainBounds bounds;

            /// How many bands the child has.
            int bands = 0;

            /**
             * Which of those bands carries alpha, or -1 for none: what
             * alpha blending resolves the child's own declaration to. Good
             * for as long as the child reaches `ImageData::set_alpha_band`
             * whenever its declaration changes.
             */
            int alpha_band = -1;
        };

        /**
         * @brief Everything a composite works out about where its children
         * sit, held together so that it can be published in one step.
         *
         * A snapshot's contents never change once it is published, which is
         * what lets a pixel lookup read one without locking while another
         * thread is working out the next one. SSim samples the terrain from
         * one thread per core inside its range-image simulation, so lookups
         * really do run concurrently.
         */
        struct GeometrySnapshot
        {
            /**
             * What is known about each child, in the same order as `images`.
             *
             * Entries for null children are present but left at their
             * defaults, so that indices line up with `images`.
             */
            std::vector<ChildGeometry> children;

            /// The merged bounds of those children.
            TerrainBounds bounds;

            /**
             * Whether every child has valid bounds, so that `bounds` is where
             * every pixel this composite can answer for is. False with no
             * children.
             */
            bool all_bounded = true;

            /**
             * The `geometry_version()` this was last known to be good for.
             *
             * Not part of what the snapshot says about the children, but a
             * record of when that was last confirmed, so it can be restamped
             * on a snapshot lookups are already reading. Atomic because they
             * are reading it at the time.
             */
            mutable std::atomic<unsigned long> version {0};
        };

        std::vector<std::shared_ptr<rsvp::ImageData> > images;

        /**
         * @brief The current snapshot, working out a new one first if an image
         * has moved or been relabelled since the last one was published.
         *
         * Resolve this once per lookup and pass it down rather than calling it
         * again for each thing it holds. Two calls can land either side of an
         * invalidation and hand back different snapshots, and a lookup that
         * blends children located by one snapshot with bounds taken from
         * another is answering from a mosaic that never existed. Doing it once
         * also keeps the atomic load off the innermost loops.
         *
         * Every pixel lookup consults this, so the check that the published
         * snapshot is still good is kept here to be inlined, and only working
         * out a new one is a call.
         */
        const GeometrySnapshot &geometry() const
        {
            const GeometrySnapshot *snapshot =
                published_geometry.load(std::memory_order_acquire);

            if (snapshot == nullptr ||
                snapshot->version.load(std::memory_order_relaxed) !=
                    geometry_version())
            {
                return refresh_geometry_cache();
            }

            return *snapshot;
        }

        /**
         * @brief Cheaply rule out a child being able to supply a clamped
         * sample at (x, y).
         *
         * Sampling a child to find out costs a walk down its chain of wrappers
         * and a bilinear fetch, and along a seam all but one or two of the
         * children are nowhere near the point, so it pays to ask this first.
         *
         * @param[in] info The child to test
         * @param[in] x    The "x-like" coordinate of the pixel of interest
         * @param[in] y    The "y-like" coordinate of the pixel of interest
         *
         * A child whose bounds or reach are unknown is never ruled out.
         *
         * @return false if the child is certainly too far from (x, y) to have
         * a say. true means only that it might.
         */
        static bool
        child_may_reach(const ChildGeometry &info, double x, double y);

    private:
        /**
         * @brief The snapshot lookups are currently reading, or null before
         * the first one has been worked out.
         *
         * Only ever made to point at a snapshot `retained_geometry` owns, so a
         * lookup that has loaded it can keep reading it for as long as this
         * composite lives.
         */
        mutable std::atomic<const GeometrySnapshot *> published_geometry {
            nullptr};

        /// Serializes working out a new snapshot. Lookups never take this.
        mutable std::mutex geometry_mutex;

        /**
         * @brief Keeps every snapshot that has been published alive.
         *
         * A lookup holds a bare reference into the current snapshot rather
         * than a share of it, since taking a share on the hot path would cost
         * an atomic increment per pixel. Retiring a snapshot therefore cannot
         * free it, so we hold on to it instead. Nothing is added here for an
         * invalidation that leaves this composite's own geometry unchanged, so
         * what accumulates is one small snapshot per real move of a child, not
         * one per lookup or one per unrelated image's move.
         *
         * @see CompositeData for what that costs a caller that moves its
         * children over and over.
         */
        mutable std::vector<std::shared_ptr<const GeometrySnapshot> >
            retained_geometry;

        /**
         * @brief Work out and publish a snapshot for the current version.
         *
         * @return The snapshot to read, which is the one just published unless
         * another thread published an equally current one first.
         */
        const GeometrySnapshot &refresh_geometry_cache() const;

        /**
         * @brief Whether two snapshots say the same thing about the children.
         *
         * Compares bit-exactly, since a snapshot that is still good is the
         * same arithmetic run over the same inputs and reproduces its own
         * values exactly.
         */
        static bool describes_same_geometry(const GeometrySnapshot &first,
                                            const GeometrySnapshot &second);

    public:
        /**
         * @brief Construct a new empty CompositeData.
         */
        CompositeData();

        ~CompositeData();

        virtual int get_alpha_band() const override;

        /**
         * @brief Add an image to the composite.
         *
         * @param img The ImageData to add to the composite
         * @param pos The index at which to add the data
         */
        void add_image(const std::shared_ptr<rsvp::ImageData> &img,
                       int pos = -1);

        /**
         * @brief Remove an image from the composite but do _not_ delete it.
         *
         * @param pos The index of the ImageData to remove
         *
         * @return A shared pointer to the removed ImageData.
         */
        std::shared_ptr<rsvp::ImageData> remove_image(int p);

        /**
         * @brief Remove an image from the composite and _do_ delete it.
         *
         * @param pos The index of the ImageData to remove
         *
         * @return true if an ImageData was removed from the list.
         */
        bool delete_image(int p);

        /**
         * @brief Get the number of ImageData objects in the composite.
         *
         * @return The number of ImageData objects in the composite.
         */
        int get_count() const;

        /**
         * @brief Get the number of bands in the composite data.
         *
         * @return The number of bands in the composite data.
         */
        virtual int get_bands() const override;

        /**
         * @brief Get the union of bounds from all images in the composite.
         *
         * The reach is the largest of the children's, or unknown if any
         * child's is, or if any child has no bounds at all: a parent
         * composite could otherwise skip this one where that child answers.
         *
         * @return The combined terrain bounds.
         */
        TerrainBounds get_bounds() const override;

        /**
         * @brief Set interpolation mode for composite and all child images.
         *
         * @param enable true to enable bilinear interpolation, false for nearest-neighbor
         */
        void set_interpolating(bool enable) override;

        /**
         * @brief Blend the edge samples of every child within a pixel of
         * (x, y), and report the largest of their weights.
         *
         * @see ImageData::get_clamped_pixel_double
         */
        bool get_clamped_pixel_double(double &value,
                                      double &weight,
                                      double x,
                                      double y,
                                      int band) const override;

    protected:
        /**
         * @brief Sample one child's data and alpha for compositing.
         *
         * A child that is certainly out of reach of (x, y) is skipped without
         * being sampled. A one-band child - a PGM - is read from band 0
         * whatever band was asked for, and called fully opaque, as is a
         * child with no alpha band.
         *
         * @param[in]  snapshot Where this composite's children sit
         * @param[in]  index    Which child to sample
         * @param[in]  x        The "x-like" coordinate of the pixel
         * @param[in]  y        The "y-like" coordinate of the pixel
         * @param[in]  band     The band of the pixel to access
         * @param[out] value    The child's data at (x, y)
         * @param[out] alpha    The child's alpha at (x, y), on the image's
         * 1-255 scale
         *
         * @return false if the child does not cover (x, y)
         *
         * @throws std::runtime_error if the child has data at (x, y) but not
         * the alpha band it declares
         */
        bool sample_child(const GeometrySnapshot &snapshot,
                          size_t index,
                          double x,
                          double y,
                          int band,
                          double &value,
                          double &alpha) const;

        /**
         * @brief Reconstruct a value in the band between abutting images.
         *
         * Tiled DEM mosaics abut without overlapping, so there is a
         * one-pixel-wide band along every seam that no single tile can
         * interpolate - each of the four bilinear corners the band needs lives
         * in a different tile. This blends the edge value of every child
         * within a pixel of (x, y), weighted by how close that edge is.
         *
         * For grids that share a pitch and a phase, which tiles cut from a
         * common mosaic do, the result is exactly the bilinear interpolation
         * the tiles would produce if they were a single image.
         *
         * This is meant as a fallback for the seam band, so callers should try
         * their normal compositing first: inside a tile this returns the same
         * answer, but more expensively.
         *
         * Where the children sit is taken as an argument rather than resolved
         * here, so that the fallback answers from the same snapshot the caller
         * already worked from.
         *
         * @param[out] value   The reconstructed value.
         * @param[in] snapshot Where this composite's children sit
         * @param[in] x        The "x-like" coordinate of the pixel
         * @param[in] y        The "y-like" coordinate of the pixel
         * @param[in] band     The band of the pixel to access
         *
         * @return false if no child is within a pixel of (x, y), which means
         * (x, y) is genuinely outside the composite rather than on a seam.
         */
        bool get_seam_pixel_double(double &value,
                                   const GeometrySnapshot &snapshot,
                                   double x,
                                   double y,
                                   int band) const;

        /**
         * @brief Cheaply rule out (x, y) being on one of this composite's
         * seams.
         *
         * A seam is a band between two images, so it takes two images to have
         * one, and it lies inside the composite rather than beyond its outer
         * edge. Both are much cheaper to check than the sample-every-child
         * loops that reconstruct a seam, and out-of-terrain lookups are common
         * enough to be worth the check.
         *
         * @param[in] snapshot Where this composite's children sit
         * @param[in] x        The "x-like" coordinate of the pixel
         * @param[in] y        The "y-like" coordinate of the pixel
         *
         * @return false if (x, y) cannot be on a seam. true means only that it
         * might be.
         */
        bool could_be_on_seam(const GeometrySnapshot &snapshot,
                              double x,
                              double y) const;

        /**
         * @brief Work out which band of an image carries its alpha value.
         *
         * Prefers what the image declares. An image that declares nothing
         * falls back on its format: a single-band PGM has no alpha, and a
         * three-band heightmap or terrain classification keeps it in band 2.
         *
         * Every composite here resolves the alpha band through this, so that a
         * seam is judged opaque or transparent by the same rule as the pixels
         * on either side of it.
         *
         * @param[in] image The image to inspect
         *
         * @return The band carrying alpha, or -1 if the image has none.
         */
        static int get_alpha_band_of(const ImageData &image);

        /**
         * @brief Sample one child of the composite for a seam reconstruction.
         *
         * Takes the child's value at the point on its edge nearest to (x, y)
         * along with the weight that point is owed, and rejects children that
         * hold no real data there.
         *
         * @param[in]  image  The child to sample
         * @param[in]  info   What is cached about `image`
         * @param[out] value  The value sampled from `image`
         * @param[out] weight The weight `value` is owed
         * @param[in]  x      The "x-like" coordinate of the pixel of interest
         * @param[in]  y      The "y-like" coordinate of the pixel of interest
         * @param[in]  band   The band of the pixel to access
         *
         * @return false if `image` is more than a pixel from (x, y), or is
         * transparent there, either of which means it does not share this
         * seam.
         */
        bool get_seam_sample(const ImageData &image,
                             const ChildGeometry &info,
                             double &value,
                             double &weight,
                             double x,
                             double y,
                             int band) const;
    };

    /**
     * @brief A class to composite heightmap-style ImageData objects together.
     *
     * Heightmap-style ImageData objects have three bands:
     *
     *  0: Real, uninterpolated height values, or 3.4e38 if no data is present.
     *  1: Interpolated height values.
     *  2: Alpha value. 255 if uniterpolated data is present, decaying by
     *     12.5/pixel away from uniterpolated data to a minimum of 1.
     *
     *  If channel 2 is 255, channels 0 and 1 will have the same value.
     *
     * To get a composited value, the images are averaged together and weighted
     * by the value in the alpha channel. An alpha value of 1 is considered to
     * be equivalent to 0.
     *
     */
    class AverageCompositeData final : public CompositeData
    {
    public:
        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };

    /**
     * @brief A class that uses alpha blending to composite heightmap-style
     * ImageData objects together.
     *
     * Heightmap-style ImageData objects have three bands:
     *
     *  0: Real, uninterpolated height values, or 3.4e38 if no data is present.
     *  1: Interpolated height values.
     *  2: Alpha value. 255 if uniterpolated data is present, decaying by
     *     12.5/pixel away from uniterpolated data to a minimum of 1.
     *
     *  If channel 2 is 255, channels 0 and 1 will have the same value.
     *
     */
    class AlphaBlendingCompositeData : public CompositeData
    {
    public:

        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };

    /**
     * @brief A class to composite terrain classification-style ImageData
     * objects.
     *
     * Terrain classification-style ImageData objects have two bands:
     *
     *  0: Integer terrain type.
     *  1: Probability score that the terrain is the given terrain type.
     *
     * To get a composited value, the probability scores for each of the images
     * are compared; the value from the image with the highest score is
     * returned unchanged.
     *
     */
    class ScoredCompositeData final : public CompositeData
    {
    public:
        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;
    };
}

#endif
