#include <composite_data.h>
#include <platform.h>
#include <translated_data.h>
#include <z_offset_data.h>

#include <cmath>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

// Image implementation for testing CompositeData classes
class CompositeTestImage : public rsvp::ImageData
{
private:
    double data[9] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    int width = 3;
    int height = 3;
    int band_count = 1;
    int alpha_value = -1;
    bool use_interpolation = true;

public:
    CompositeTestImage() = default;

    // Create multi-band image
    explicit CompositeTestImage(int bands) :
        band_count(bands)
    {
    }

    // Set interpolation flag
    void set_interpolating(bool enable) override
    {
        use_interpolation = enable;
    }

    int get_interpolating() const override
    {
        return use_interpolation;
    }

    int get_bands() const override
    {
        return band_count;
    }

    bool get_pixel_double(double &value, int x, int y, int band) const override
    {
        if (x < 0 || x >= width || y < 0 || y >= height || band >= band_count)
        {
            return false;
        }
        
        // Handle negative band indices by using band 0, to match the comment in the test
        int actual_band = (band < 0) ? 0 : band;
        
        if (actual_band == 0)
        {
            value = data[y * width + x];
        }
        else if (actual_band == 1)
        {
            // Second band contains values 10-19
            value = 10.0 + (y * width + x);
        }
        else if (actual_band == 2)
        {
            // Alpha band contains values 100-109 (for testing)
            value = 100.0 + (y * width + x);
        }
        else
        {
            // For any other band
            value = actual_band * 100.0 + (y * width + x);
        }
        return true;
    }

    bool get_interpolated_pixel_double(double &value,
                                       double x,
                                       double y,
                                       int band) const override
    {
        if (!use_interpolation)
        {
            return get_pixel_double(value,
                                    static_cast<int>(x + 0.5),
                                    static_cast<int>(y + 0.5),
                                    band);
        }

        // Simple implementation that does basic bilinear interpolation
        int x0 = static_cast<int>(x);
        int y0 = static_cast<int>(y);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        double dx = x - x0;
        double dy = y - y0;

        double v00 = 0.0, v01 = 0.0, v10 = 0.0, v11 = 0.0;

        if (!get_pixel_double(v00, x0, y0, band) ||
            !get_pixel_double(v01, x0, y1, band) ||
            !get_pixel_double(v10, x1, y0, band) ||
            !get_pixel_double(v11, x1, y1, band))
        {
            return false;
        }

        value = v00 * (1 - dx) * (1 - dy) + v01 * (1 - dx) * dy +
            v10 * dx * (1 - dy) + v11 * dx * dy;

        return true;
    }

    bool get_pixel_int(int &value, int x, int y, int band) const override
    {
        double d_value;
        if (!get_pixel_double(d_value, x, y, band))
        {
            return false;
        }
        value = static_cast<int>(d_value);
        return true;
    }

    bool get_interpolated_pixel_int(int &value,
                                    double x,
                                    double y,
                                    int band) const override
    {
        double d_value;
        if (!get_interpolated_pixel_double(d_value, x, y, band))
        {
            return false;
        }
        value = static_cast<int>(d_value);
        return true;
    }

    int get_width() const override
    {
        return width;
    }

    int get_height() const override
    {
        return height;
    }

    int get_alpha_band() const override
    {
        return alpha_value;
    }

    void set_alpha_band(int band) override
    {
        alpha_value = band;
    }
};

// Test CompositeData base class
// Flat image for feathering tests: constant height in bands 0 and 1 and a
// constant alpha in band 2. Uses the base class bilinear interpolation.
class FlatTestImage : public rsvp::ImageData
{
private:
    int width;
    int height;
    double height_value;
    double alpha_value;

public:
    FlatTestImage(int w, int h, double height_val, double alpha_val) :
        width(w), height(h), height_value(height_val), alpha_value(alpha_val)
    {
        set_alpha_band(2);
    }

    int get_bands() const override
    {
        return 3;
    }

    bool get_pixel_double(double &value, int x, int y, int band) const override
    {
        if (x < 0 || y < 0 || x >= width || y >= height || band < 0 ||
            band >= 3)
        {
            return false;
        }
        value = (band == 2) ? alpha_value : height_value;
        return true;
    }

    int get_width() const override
    {
        return width;
    }

    int get_height() const override
    {
        return height;
    }
};

TEST(composite_data, base_class_functions)
{
    // Create a concrete class that inherits from CompositeData
    class TestCompositeData : public rsvp::CompositeData
    {
    public:
        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            return false;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            return false;
        }
    };

    // Create an instance of our test class
    TestCompositeData composite;

    // Test empty composite
    EXPECT_EQ(composite.get_count(), 0);
    EXPECT_EQ(composite.get_bands(), 0);
    EXPECT_EQ(composite.get_alpha_band(), -1); // Default alpha band

    double value = 0.0;
    EXPECT_FALSE(composite.get_pixel_double(value, 0, 0, 0));
    EXPECT_FALSE(composite.get_interpolated_pixel_double(value, 0.0, 0.0, 0));

    // Add an image to the composite
    auto img1 = std::make_shared<CompositeTestImage>();
    composite.add_image(img1);

    // Test after adding one image
    EXPECT_EQ(composite.get_count(), 1);
    EXPECT_EQ(composite.get_bands(), 1);
    EXPECT_EQ(composite.get_alpha_band(),
              -1); // Should match the image's alpha band

    // Add a second image at a specific position
    auto img2 = std::make_shared<CompositeTestImage>(1); // 1-band image
    img2->set_alpha_band(0);
    composite.add_image(img2, 0); // Insert at beginning

    // Test after adding second image
    EXPECT_EQ(composite.get_count(), 2);
    EXPECT_EQ(composite.get_bands(),
              1); // Should match the first image in the vector
    EXPECT_EQ(composite.get_alpha_band(),
              0); // Should match the first image's alpha band

    // Add a nullptr (should be ignored)
    composite.add_image(nullptr);
    EXPECT_EQ(composite.get_count(), 2); // Count should not change

    // Add a third image out of bounds (should add to the end)
    auto img3 = std::make_shared<CompositeTestImage>();
    composite.add_image(img3, 10);
    EXPECT_EQ(composite.get_count(), 3);

    // Remove middle image (but don't delete it)
    auto removed = composite.remove_image(1);
    EXPECT_EQ(composite.get_count(), 2);
    EXPECT_EQ(removed, img1);

    // Try to remove out of bounds (should return nullptr)
    removed = composite.remove_image(10);
    EXPECT_EQ(composite.get_count(), 2);
    EXPECT_EQ(removed, nullptr);

    // Delete image at valid index
    bool result = composite.delete_image(0);
    EXPECT_TRUE(result);
    EXPECT_EQ(composite.get_count(), 1);

    // Try to delete out of bounds
    result = composite.delete_image(10);
    EXPECT_FALSE(result);
    EXPECT_EQ(composite.get_count(), 1);
}

// Test AverageCompositeData class
TEST(composite_data, average_composite_data)
{
    rsvp::AverageCompositeData composite;

    // Test empty composite first
    double value = 0.0;
    EXPECT_FALSE(composite.get_interpolated_pixel_double(value, 0.0, 0.0, 0));

    // Now add an image with valid pixel data and test again
    auto img = std::make_shared<CompositeTestImage>(1);
    img->set_alpha_band(
        0); // Use the only band as alpha band to ensure blending works
    composite.add_image(img);

    // Even with a valid image added, the Average composite may not be able to
    // produce a result if the alpha value is too small, so we don't expect a
    // specific result

    // Test with out of bounds coordinates
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, -1.0, -1.0, 0));
}

// Test AlphaBlendingCompositeData class
TEST(composite_data, alpha_blending_composite_data)
{
    rsvp::AlphaBlendingCompositeData composite;

    // Create and add test images
    auto img1 = std::make_shared<CompositeTestImage>(3);
    auto img2 = std::make_shared<CompositeTestImage>(3);

    // Manually set alpha band
    img1->set_alpha_band(2);
    img2->set_alpha_band(2);

    composite.add_image(img1);
    composite.add_image(img2);

    double value = 0.0;

    // Test get_pixel_double with valid coordinates
    EXPECT_TRUE(composite.get_pixel_double(value, 1, 1, 0));

    // Test get_interpolated_pixel_double with valid coordinates
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 1.5, 1.5, 0));

    // Test with out of bounds coordinates
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, 10.0, 10.0, 0));

    // AlphaBlendingCompositeData handles invalid band indices by using band 0
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 0.0, 0.0, -1));

    // Test with single band image (PGM data case)
    rsvp::AlphaBlendingCompositeData pgmComposite;
    auto pgmImage = std::make_shared<CompositeTestImage>(1);
    pgmComposite.add_image(pgmImage);

    EXPECT_TRUE(
        pgmComposite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));

    // Test with invalid alpha band
    auto invalidAlphaImg = std::make_shared<CompositeTestImage>(3);
    invalidAlphaImg->set_alpha_band(-1);

    rsvp::AlphaBlendingCompositeData invalidAlphaComposite;
    invalidAlphaComposite.add_image(invalidAlphaImg);

    EXPECT_TRUE(invalidAlphaComposite.get_interpolated_pixel_double(
        value, 1.0, 1.0, 0));
}

// Test ScoredCompositeData class
TEST(composite_data, scored_composite_data)
{
    rsvp::ScoredCompositeData composite;

    // Test empty composite first
    double value = 0.0;
    EXPECT_FALSE(composite.get_interpolated_pixel_double(value, 0.0, 0.0, 0));

    // Now add an image with valid pixel data
    auto img = std::make_shared<CompositeTestImage>(1);
    // Set the only band as alpha band to ensure scoring works
    img->set_alpha_band(0);
    composite.add_image(img);

    // Now with a valid image added, we should get valid results for normal
    // coordinates
    EXPECT_TRUE(composite.get_pixel_double(value, 1, 1, 0));
}

// Test interpolation passthrough in CompositeData
TEST(composite_data, interpolation_passthrough)
{
    rsvp::AlphaBlendingCompositeData composite;

    // Create test images with interpolation enabled
    auto img1 = std::make_shared<CompositeTestImage>(3);
    auto img2 = std::make_shared<CompositeTestImage>(3);

    img1->set_alpha_band(2);
    img2->set_alpha_band(2);

    // Initially, interpolation should be enabled
    EXPECT_TRUE(img1->get_interpolating());
    EXPECT_TRUE(img2->get_interpolating());

    composite.add_image(img1);
    composite.add_image(img2);

    // Disable interpolation on composite
    composite.set_interpolating(false);

    // Verify interpolation flag propagated to child images
    EXPECT_FALSE(img1->get_interpolating());
    EXPECT_FALSE(img2->get_interpolating());

    // Re-enable interpolation on composite
    composite.set_interpolating(true);

    // Verify flag propagated again
    EXPECT_TRUE(img1->get_interpolating());
    EXPECT_TRUE(img2->get_interpolating());
}

// Test get_edge_distance on the base class and the wrappers that forward it
TEST(composite_data, edge_distance)
{
    // Default implementation: distance to the edge of the bilinear domain
    // [0, width - 1) x [0, height - 1)
    auto img = std::make_shared<CompositeTestImage>(3);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(1.0, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(0.0, 0.5), 0.0);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(1.5, 1.5), 0.5);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(0.25, 1.0), 0.25);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(-1.0, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(img->get_edge_distance(2.5, 1.0), 0.0);

    // Unknown size: never feather
    class UnknownSizeImage : public CompositeTestImage
    {
    public:
        int get_width() const override
        {
            return 0;
        }
    };
    UnknownSizeImage unknown;
    EXPECT_TRUE(std::isinf(unknown.get_edge_distance(1.0, 1.0)));

    // TranslatedData maps world coordinates back to child pixels first
    rsvp::TranslatedData translated(img, 10.0, 20.0, 2.0, 0.0);
    EXPECT_DOUBLE_EQ(translated.get_edge_distance(12.0, 22.0), 1.0);
    EXPECT_DOUBLE_EQ(translated.get_edge_distance(10.0, 21.0), 0.0);
    EXPECT_DOUBLE_EQ(translated.get_edge_distance(13.0, 23.0), 0.5);

    // ZOffsetData forwards
    rsvp::ZOffsetData offset(img);
    EXPECT_DOUBLE_EQ(offset.get_edge_distance(1.0, 1.0), 1.0);

    // CompositeData returns the largest child distance
    rsvp::AlphaBlendingCompositeData composite;
    EXPECT_DOUBLE_EQ(composite.get_edge_distance(1.0, 1.0), 0.0);
    composite.add_image(img);
    composite.add_image(std::make_shared<FlatTestImage>(11, 11, 1.0, 255.0));
    EXPECT_DOUBLE_EQ(composite.get_edge_distance(1.0, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(composite.get_edge_distance(5.0, 5.0), 5.0);
}

// Test that two overlapping rectangular tiles are feathered into each other
// (this mimics orbital DEM tiling, where alpha is 255 right up to the border)
TEST(composite_data, feathered_blend_of_flat_tiles)
{
    // Tile A covers x in [0, 63), tile B covers x in [48, 111): 15 px overlap
    auto tile_a = std::make_shared<FlatTestImage>(64, 64, 100.0, 255.0);
    auto tile_b = std::make_shared<rsvp::TranslatedData>(
        std::make_shared<FlatTestImage>(64, 64, 50.0, 255.0), 48.0, 0.0, 1.0,
        0.0);

    rsvp::AlphaBlendingCompositeData ab;
    ab.add_image(tile_a);
    ab.add_image(tile_b);

    rsvp::AlphaBlendingCompositeData ba;
    ba.add_image(tile_b);
    ba.add_image(tile_a);

    const double y = 31.5;
    double value = 0.0;

    // Single coverage returns the tile's value exactly, even near a border
    EXPECT_TRUE(ab.get_interpolated_pixel_double(value, 5.0, y, 1));
    EXPECT_DOUBLE_EQ(value, 100.0);
    EXPECT_TRUE(ab.get_interpolated_pixel_double(value, 0.0, y, 1));
    EXPECT_DOUBLE_EQ(value, 100.0);
    EXPECT_TRUE(ab.get_interpolated_pixel_double(value, 100.0, y, 1));
    EXPECT_DOUBLE_EQ(value, 50.0);
    EXPECT_FALSE(ab.get_interpolated_pixel_double(value, 120.0, y, 1));

    // Walk across the overlap: the composite ramps monotonically from 100 to
    // 50 with no steps larger than the feather slope allows.
    // Inside the overlap both feathers are linear with slope 1/20 per px and
    // their sum is constant, so the composite is linear with slope
    // 50 / 15 per px.
    const double step = 0.1;
    const double max_allowed_jump = (50.0 / 15.0) * step + 1e-6;
    double previous = 100.0;
    for (double x = 40.0; x <= 70.0; x += step)
    {
        EXPECT_TRUE(ab.get_interpolated_pixel_double(value, x, y, 1)) << x;
        EXPECT_LE(value, previous + 1e-9) << x;
        EXPECT_GE(value, 50.0 - 1e-9) << x;
        EXPECT_LE(previous - value, max_allowed_jump) << x;

        // Order of the tiles does not matter
        double reversed = 0.0;
        EXPECT_TRUE(ba.get_interpolated_pixel_double(reversed, x, y, 1));
        EXPECT_DOUBLE_EQ(value, reversed) << x;

        previous = value;
    }

    // Midway through the overlap the tiles are weighted equally
    EXPECT_TRUE(ab.get_interpolated_pixel_double(value, 55.5, y, 1));
    EXPECT_NEAR(value, 75.0, 1e-9);

    // Nearest-neighbor mode still blends and stays within the tile values
    ab.set_interpolating(false);
    EXPECT_TRUE(ab.get_interpolated_pixel_double(value, 55.5, y, 1));
    EXPECT_GE(value, 50.0);
    EXPECT_LE(value, 100.0);
    ab.set_interpolating(true);

    // Tiles with identical data (orbital tiles share their edge pixels)
    // composite to exactly that data everywhere
    auto same_a = std::make_shared<FlatTestImage>(64, 64, 100.0, 255.0);
    auto same_b = std::make_shared<rsvp::TranslatedData>(
        std::make_shared<FlatTestImage>(64, 64, 100.0, 255.0), 48.0, 0.0,
        1.0, 0.0);
    rsvp::AlphaBlendingCompositeData same;
    same.add_image(same_a);
    same.add_image(same_b);
    for (double x = 0.0; x < 111.0; x += 0.37)
    {
        EXPECT_TRUE(same.get_interpolated_pixel_double(value, x, y, 1)) << x;
        EXPECT_DOUBLE_EQ(value, 100.0) << x;
    }
}

// Test the feather width setting
TEST(composite_data, feather_width_setter)
{
    auto tile_a = std::make_shared<FlatTestImage>(64, 64, 100.0, 255.0);
    auto tile_b = std::make_shared<rsvp::TranslatedData>(
        std::make_shared<FlatTestImage>(64, 64, 50.0, 255.0), 48.0, 0.0, 1.0,
        0.0);

    rsvp::AlphaBlendingCompositeData composite;
    composite.add_image(tile_a);
    composite.add_image(tile_b);

    EXPECT_DOUBLE_EQ(composite.get_feather_width(),
                     rsvp::AlphaBlendingCompositeData::kDefaultFeatherWidth);
    EXPECT_DOUBLE_EQ(composite.get_feather_width(), 20.0);

    // At x = 50, tile A is 13 px from its edge and tile B is 2 px from its
    // edge: weights 0.65 and 0.10
    double value = 0.0;
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 50.0, 31.5, 1));
    EXPECT_NEAR(value, (100.0 * 0.65 + 50.0 * 0.10) / 0.75, 1e-9);

    // A narrower feather saturates both weights at 1 there
    composite.set_feather_width(2.0);
    EXPECT_DOUBLE_EQ(composite.get_feather_width(), 2.0);
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 50.0, 31.5, 1));
    EXPECT_NEAR(value, 75.0, 1e-9);

    // Zero disables border feathering entirely
    composite.set_feather_width(0.0);
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 48.5, 31.5, 1));
    EXPECT_NEAR(value, 75.0, 1e-9);
}

// Test that the alpha weighting is continuous (no cliff near alpha 255)
TEST(composite_data, alpha_weighting_is_continuous)
{
    auto real = std::make_shared<FlatTestImage>(64, 64, 100.0, 255.0);

    // Two extrapolated tiles whose alpha differ by less than one unit. The old
    // compositor treated 254.95 as opaque (weight 1.0) and 254.85 as
    // extrapolated (weight ~0.001), a thousandfold cliff.
    auto nearly_real_a = std::make_shared<FlatTestImage>(64, 64, 50.0, 254.95);
    auto nearly_real_b = std::make_shared<FlatTestImage>(64, 64, 50.0, 254.85);

    double value_a = 0.0;
    double value_b = 0.0;
    {
        rsvp::AlphaBlendingCompositeData composite;
        composite.add_image(real);
        composite.add_image(nearly_real_a);
        EXPECT_TRUE(
            composite.get_interpolated_pixel_double(value_a, 31.5, 31.5, 1));
    }
    {
        rsvp::AlphaBlendingCompositeData composite;
        composite.add_image(real);
        composite.add_image(nearly_real_b);
        EXPECT_TRUE(
            composite.get_interpolated_pixel_double(value_b, 31.5, 31.5, 1));
    }

    // Both should be close to the plain mean, and close to each other
    EXPECT_NEAR(value_a, 75.0, 0.1);
    EXPECT_NEAR(value_b, 75.0, 0.1);
    EXPECT_NEAR(value_a, value_b, 0.05);

    // Alpha at the floor (1) means no data: the tile is ignored entirely
    auto no_data = std::make_shared<FlatTestImage>(64, 64, 50.0, 1.0);
    rsvp::AlphaBlendingCompositeData composite;
    composite.add_image(no_data);
    composite.add_image(real);
    EXPECT_TRUE(
        composite.get_interpolated_pixel_double(value_a, 31.5, 31.5, 1));
    EXPECT_DOUBLE_EQ(value_a, 100.0);

    rsvp::AlphaBlendingCompositeData only_no_data;
    only_no_data.add_image(no_data);
    EXPECT_FALSE(
        only_no_data.get_interpolated_pixel_double(value_a, 31.5, 31.5, 1));
}

// Test get_bounds for CompositeData
TEST(composite_data, composite_bounds)
{
    // Create test image with known bounds
    class BoundsTestImage : public CompositeTestImage
    {
    public:
        rsvp::TerrainBounds get_bounds() const override
        {
            rsvp::TerrainBounds bounds;
            bounds.valid = true;
            bounds.min_x = 10.0;
            bounds.max_x = 20.0;
            bounds.min_y = 30.0;
            bounds.max_y = 40.0;
            return bounds;
        }
    };

    rsvp::AlphaBlendingCompositeData composite;

    // Empty composite should return invalid bounds
    auto bounds = composite.get_bounds();
    EXPECT_FALSE(bounds.valid);

    // Add first image
    auto img1 = std::make_shared<BoundsTestImage>();
    composite.add_image(img1);

    bounds = composite.get_bounds();
    EXPECT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0);
    EXPECT_DOUBLE_EQ(bounds.max_x, 20.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, 30.0);
    EXPECT_DOUBLE_EQ(bounds.max_y, 40.0);

    // Add second image with different bounds
    class BoundsTestImage2 : public CompositeTestImage
    {
    public:
        rsvp::TerrainBounds get_bounds() const override
        {
            rsvp::TerrainBounds bounds;
            bounds.valid = true;
            bounds.min_x = 15.0;
            bounds.max_x = 25.0;
            bounds.min_y = 25.0;
            bounds.max_y = 35.0;
            return bounds;
        }
    };

    auto img2 = std::make_shared<BoundsTestImage2>();
    composite.add_image(img2);

    // Bounds should be the union of both images
    bounds = composite.get_bounds();
    EXPECT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0);  // min from img1
    EXPECT_DOUBLE_EQ(bounds.max_x, 25.0);  // max from img2
    EXPECT_DOUBLE_EQ(bounds.min_y, 25.0);  // min from img2
    EXPECT_DOUBLE_EQ(bounds.max_y, 40.0);  // max from img1
}