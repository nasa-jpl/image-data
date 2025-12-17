#ifndef RSVP_IMAGE_DATA_FEATHERED_COMPOSITE_DATA_H
#define RSVP_IMAGE_DATA_FEATHERED_COMPOSITE_DATA_H

#include "composite_data.h"

#include <memory>
#include <vector>


namespace rsvp
{

    /**
     * @brief A class that composites terrain heightmaps with feathering.
     *
     * This class implements a specialized compositing mode for terrain data
     * where orbital terrains (identified by filename) are treated as fallback
     * data. Non-orbital terrains are feathered at their outer boundaries to
     * smoothly transition to orbital terrains, while avoiding feathering at
     * holes where non-orbital terrains meet each other.
     *
     * Feathering uses a distance field approach to distinguish outer edges
     * from interior holes, and applies a smoothstep function for smooth
     * transitions.
     */
    class FeatheredCompositeData : public CompositeData
    {
    private:
        // Separate lists for non-orbital and orbital terrains
        std::vector<std::shared_ptr<rsvp::ImageData>> non_orbital_images_;
        std::vector<std::shared_ptr<rsvp::ImageData>> orbital_images_;

        // Feathering distance in meters
        double feather_distance_;

        /**
         * @brief Smoothstep interpolation function.
         *
         * Provides a smooth S-curve transition with zero derivatives at
         * endpoints.
         *
         * @param t Input value in range [0, 1]
         * @return Smoothstep value in range [0, 1]
         */
        static double smoothstep(double t);

        /**
         * @brief Compute distance to nearest edge for a terrain.
         *
         * Computes the signed distance from a query point to the nearest
         * solid/extrapolated boundary in a terrain. Negative distance means
         * inside solid region, positive means in extrapolated region.
         *
         * @param image The terrain image to query
         * @param x X coordinate in meters
         * @param y Y coordinate in meters
         * @return Signed distance to nearest edge, or large positive value if
         * no data
         */
        double compute_distance_to_edge(
            const std::shared_ptr<rsvp::ImageData> &image,
            double x,
            double y) const;

        /**
         * @brief Check if any non-orbital terrain has solid data at a point.
         *
         * @param x X coordinate in meters
         * @param y Y coordinate in meters
         * @param exclude_image Image to exclude from check (to detect holes)
         * @return true if any other non-orbital terrain has solid data
         */
        bool has_other_non_orbital_data(
            double x,
            double y,
            const std::shared_ptr<rsvp::ImageData> &exclude_image) const;

        /**
         * @brief Compute feathering weight based on distance field.
         *
         * Determines how much to weight non-orbital vs orbital data based on
         * distance to edges and presence of other non-orbital data.
         *
         * @param x X coordinate in meters
         * @param y Y coordinate in meters
         * @return Weight in [0, 1], where 1.0 = full non-orbital, 0.0 = full
         * orbital
         */
        double compute_feathering_weight(double x, double y) const;

        /**
         * @brief Blend non-orbital terrains using alpha blending.
         *
         * @param value Output blended height value
         * @param x X coordinate in meters
         * @param y Y coordinate in meters
         * @param band Band to query
         * @return true if valid data found
         */
        bool blend_non_orbital_terrains(double &value,
                                        double x,
                                        double y,
                                        int band) const;

        /**
         * @brief Blend orbital terrains using alpha blending.
         *
         * @param value Output blended height value
         * @param x X coordinate in meters
         * @param y Y coordinate in meters
         * @param band Band to query
         * @return true if valid data found
         */
        bool blend_orbital_terrains(double &value,
                                    double x,
                                    double y,
                                    int band) const;

    public:
        /**
         * @brief Construct a new FeatheredCompositeData.
         *
         * @param feather_distance Distance in meters over which to feather
         */
        explicit FeatheredCompositeData(double feather_distance);

        ~FeatheredCompositeData();

        /**
         * @brief Add an image to the composite.
         *
         * @param img The ImageData to add
         * @param is_orbital Whether this is an orbital terrain
         * @param pos The index at which to add the data (-1 for end)
         */
        void add_image(const std::shared_ptr<rsvp::ImageData> &img,
                       bool is_orbital,
                       int pos = -1);

        // Return an interpolated pixel value as a double
        virtual bool get_interpolated_pixel_double(
            double &value,
            const double x,
            const double y,
            const int band) const override;

        // Return an exact pixel value as a double
        virtual bool get_pixel_double(double &value,
                                      const int x,
                                      const int y,
                                      const int band) const override;
    };

} // namespace rsvp

#endif
