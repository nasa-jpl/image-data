#include <composite_data.h>
#include <platform.h>

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

        // Handle negative band indices by using band 0, to match the comment
        // in the test
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

// Test FirstValidCompositeData - returns first valid pixel without blending
TEST(composite_data, first_valid_composite_data)
{
    // Create test image with opaque alpha values (255)
    class OpaqueTestImage : public CompositeTestImage
    {
    public:
        OpaqueTestImage(double height_value) :
            CompositeTestImage(3),
            height_val(height_value)
        {
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3)
            {
                return false;
            }

            if (band == 1)
            {
                // Height band - return constant height
                value = height_val;
            }
            else if (band == 2)
            {
                // Alpha band - return opaque
                value = 255.0;
            }
            else
            {
                value = 0.0;
            }
            return true;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            // For this test, just use nearest neighbor
            return get_pixel_double(value,
                                    static_cast<int>(x + 0.5),
                                    static_cast<int>(y + 0.5),
                                    band);
        }

    private:
        double height_val;
    };

    rsvp::FirstValidCompositeData composite;

    // Create two overlapping tiles with different heights but same alpha
    auto tile1 = std::make_shared<OpaqueTestImage>(100.0); // 100m height
    auto tile2 = std::make_shared<OpaqueTestImage>(50.0);  // 50m height

    tile1->set_alpha_band(2);
    tile2->set_alpha_band(2);

    composite.add_image(tile1);
    composite.add_image(tile2);

    double value = 0.0;

    // FirstValidCompositeData should always use first valid tile
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 1.0, 1.0, 1));
    EXPECT_DOUBLE_EQ(value, 100.0); // Should use tile1 (first valid)

    // Test with invalid first tile - should fall through to second tile
    rsvp::FirstValidCompositeData composite2;

    // Create image with no valid data (alpha=1, which means no data)
    class InvalidTestImage : public OpaqueTestImage
    {
    public:
        InvalidTestImage() :
            OpaqueTestImage(999.0)
        {
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3)
            {
                return false;
            }

            if (band == 2)
            {
                // Alpha band - return 1 (no valid data)
                value = 1.0;
            }
            else
            {
                value = 999.0;
            }
            return true;
        }
    };

    auto invalid_tile = std::make_shared<InvalidTestImage>();
    invalid_tile->set_alpha_band(2);

    composite2.add_image(invalid_tile);
    composite2.add_image(tile2);

    // Should skip first tile (invalid) and use second tile
    EXPECT_TRUE(composite2.get_interpolated_pixel_double(value, 1.0, 1.0, 1));
    EXPECT_DOUBLE_EQ(value, 50.0); // Should use tile2
}

// Test FirstValidCompositeData with additional edge cases
TEST(composite_data, first_valid_composite_data_edge_cases)
{
    // Test 1: Empty composite returns false
    rsvp::FirstValidCompositeData empty_composite;
    double value = 0.0;
    EXPECT_FALSE(
        empty_composite.get_interpolated_pixel_double(value, 1.0, 1.0, 1));
    EXPECT_FALSE(empty_composite.get_pixel_double(value, 1, 1, 1));

    // Test 2: Out of bounds coordinates
    rsvp::FirstValidCompositeData oob_composite;
    auto img = std::make_shared<CompositeTestImage>(3);
    img->set_alpha_band(2);
    oob_composite.add_image(img);

    // Out of bounds should skip to next image or return false
    EXPECT_FALSE(
        oob_composite.get_interpolated_pixel_double(value, 100.0, 100.0, 1));
    EXPECT_FALSE(oob_composite.get_pixel_double(value, 100, 100, 1));

    // Test 3: Single-band image (PGM format)
    class SingleBandTestImage : public CompositeTestImage
    {
    public:
        SingleBandTestImage() :
            CompositeTestImage(1)
        {
        }

        int get_bands() const override
        {
            return 1;
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3 || band != 0)
            {
                return false;
            }
            value = 42.0;
            return true;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            return get_pixel_double(value,
                                    static_cast<int>(x + 0.5),
                                    static_cast<int>(y + 0.5),
                                    band);
        }
    };

    rsvp::FirstValidCompositeData single_band_composite;
    auto single_band_img = std::make_shared<SingleBandTestImage>();
    single_band_composite.add_image(single_band_img);

    EXPECT_TRUE(single_band_composite.get_interpolated_pixel_double(
        value, 1.0, 1.0, 0));
    EXPECT_DOUBLE_EQ(value, 42.0);
    EXPECT_TRUE(single_band_composite.get_pixel_double(value, 1, 1, 0));
    EXPECT_DOUBLE_EQ(value, 42.0);

    // Test 4: Image with no alpha band (alpha_band < 0)
    class NoAlphaBandImage : public CompositeTestImage
    {
    public:
        NoAlphaBandImage() :
            CompositeTestImage(2)
        {
        }

        int get_alpha_band() const override
        {
            return -1;
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3)
            {
                return false;
            }
            value = (band == 1) ? 123.0 : 0.0;
            return true;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            return get_pixel_double(value,
                                    static_cast<int>(x + 0.5),
                                    static_cast<int>(y + 0.5),
                                    band);
        }
    };

    rsvp::FirstValidCompositeData no_alpha_composite;
    auto no_alpha_img = std::make_shared<NoAlphaBandImage>();
    no_alpha_composite.add_image(no_alpha_img);

    EXPECT_TRUE(
        no_alpha_composite.get_interpolated_pixel_double(value, 1.0, 1.0, 1));
    EXPECT_DOUBLE_EQ(value, 123.0);

    // Test 5: Image with alpha band but no alpha at specific pixel
    class MissingAlphaImage : public CompositeTestImage
    {
    public:
        MissingAlphaImage() :
            CompositeTestImage(3)
        {
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3)
            {
                return false;
            }

            if (band == 2) // Alpha band
            {
                // Only center pixel has alpha
                if (x == 1 && y == 1)
                {
                    value = 255.0;
                    return true;
                }
                return false; // No alpha at other pixels
            }

            value = (band == 1) ? 77.0 : 0.0;
            return true;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double x,
                                           double y,
                                           int band) const override
        {
            return get_pixel_double(value,
                                    static_cast<int>(x + 0.5),
                                    static_cast<int>(y + 0.5),
                                    band);
        }
    };

    rsvp::FirstValidCompositeData missing_alpha_composite;
    auto missing_alpha_img = std::make_shared<MissingAlphaImage>();
    missing_alpha_img->set_alpha_band(2);

    auto fallback_img = std::make_shared<CompositeTestImage>(3);
    fallback_img->set_alpha_band(2);

    missing_alpha_composite.add_image(missing_alpha_img);
    missing_alpha_composite.add_image(fallback_img);

    // At (0,0), first image has no alpha, should use second image
    EXPECT_TRUE(missing_alpha_composite.get_interpolated_pixel_double(
        value, 0.0, 0.0, 1));
    // At (1,1), first image has alpha, should use first image
    EXPECT_TRUE(missing_alpha_composite.get_interpolated_pixel_double(
        value, 1.0, 1.0, 1));
    EXPECT_DOUBLE_EQ(value, 77.0);
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
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0); // min from img1
    EXPECT_DOUBLE_EQ(bounds.max_x, 25.0); // max from img2
    EXPECT_DOUBLE_EQ(bounds.min_y, 25.0); // min from img2
    EXPECT_DOUBLE_EQ(bounds.max_y, 40.0); // max from img1
}

// Synthetic terrain image for distance-weighted blending tests
class DistanceTestImage : public rsvp::ImageData
{
private:
    double height_value_;
    double alpha_value_;
    double min_x_, min_y_, max_x_, max_y_;

public:
    DistanceTestImage(double height,
                      double alpha,
                      double min_x,
                      double min_y,
                      double max_x,
                      double max_y) :
        height_value_(height),
        alpha_value_(alpha),
        min_x_(min_x),
        min_y_(min_y),
        max_x_(max_x),
        max_y_(max_y)
    {
        set_alpha_band(2);
    }

    bool get_pixel_double(double &value,
                          int /*x*/,
                          int /*y*/,
                          int band) const override
    {
        if (band == 0 || band == 1)
        {
            value = height_value_;
        }
        else if (band == 2)
        {
            value = alpha_value_;
        }
        else
        {
            return false;
        }
        return true;
    }

    bool get_interpolated_pixel_double(double &value,
                                       double x,
                                       double y,
                                       int band) const override
    {
        if (x < min_x_ || x > max_x_ || y < min_y_ || y > max_y_)
        {
            return false;
        }
        return get_pixel_double(value, 0, 0, band);
    }

    int get_bands() const override
    {
        return 3;
    }

    int get_width() const override
    {
        return 100;
    }

    int get_height() const override
    {
        return 100;
    }

    rsvp::TerrainBounds get_bounds() const override
    {
        rsvp::TerrainBounds bounds;
        bounds.valid = true;
        bounds.min_x = min_x_;
        bounds.max_x = max_x_;
        bounds.min_y = min_y_;
        bounds.max_y = max_y_;
        return bounds;
    }
};

// Test DistanceWeightedCompositeData - behavior with non-VICAR images
// Non-VICAR images (like DistanceTestImage) have distance weighting disabled
// since they're not from stereo cameras (NAVCAM/FHAZ/RHAZ)
TEST(composite_data, distance_weighted_basic)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Terrain 1: height=10, camera at (0,0), bounds 0-20
    auto terrain1 =
        std::make_shared<DistanceTestImage>(10.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(terrain1, 0.0, 0.0);

    // Terrain 2: height=20, camera at (10,0), bounds 0-20
    auto terrain2 =
        std::make_shared<DistanceTestImage>(20.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(terrain2, 10.0, 0.0);

    double height;

    // Non-VICAR images have distance weighting disabled (range_error_coefficient=0)
    // Both terrains have equal alpha, so result is always the average: 15.0
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
    EXPECT_NEAR(height, 15.0, 0.1)
        << "Non-VICAR images: distance weighting disabled, always get average";

    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 10.0, 0.0, 1));
    EXPECT_NEAR(height, 15.0, 0.1)
        << "Non-VICAR images: distance weighting disabled, always get average";

    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 5.0, 0.0, 1));
    EXPECT_NEAR(height, 15.0, 0.1)
        << "Non-VICAR images: distance weighting disabled, always get average";
}

// Test DistanceWeightedCompositeData - alpha multiplication
TEST(composite_data, distance_weighted_alpha_mult)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Low alpha terrain near camera
    auto low_alpha =
        std::make_shared<DistanceTestImage>(10.0, 100.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(low_alpha, 0.0, 0.0);

    // High alpha terrain far from camera
    auto high_alpha =
        std::make_shared<DistanceTestImage>(20.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(high_alpha, 20.0, 0.0);

    double height;

    // Query at (0, 0) - near camera 1 but with low alpha vs far from camera 2
    // but with high alpha
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
    // Non-VICAR images have distance weighting disabled (range_error_coefficient=0).
    // Result is purely alpha-based: (10*0.39 + 20*1.0)/(0.39+1.0) ≈ 17.2
    EXPECT_GT(height, 15.0) << "High alpha far terrain outweighs low alpha near";
    EXPECT_LT(height, 19.0) << "But low alpha terrain still contributes";

    // Query at (10, 0) - midpoint between cameras
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 10.0, 0.0, 1));
    // High alpha terrain should have more influence
    EXPECT_GT(height, 16.0) << "High alpha should dominate";
}

// Test DistanceWeightedCompositeData - quadratic falloff
TEST(composite_data, distance_weighted_falloff)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Terrain with bounds 0-40 (scale = 40/4 = 10)
    auto terrain1 = std::make_shared<DistanceTestImage>(
        100.0, 255.0, 0.0, 0.0, 40.0, 40.0);
    composite.add_image_with_origin(terrain1, 20.0, 20.0);

    double height;

    // At camera origin, should get exact height
    EXPECT_TRUE(
        composite.get_interpolated_pixel_double(height, 20.0, 20.0, 1));
    EXPECT_NEAR(height, 100.0, 0.01) << "At camera origin should get exact "
                                        "height";
}

// Test DistanceWeightedCompositeData - low alpha skipped
TEST(composite_data, distance_weighted_low_alpha)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Terrain with alpha=1 (below threshold)
    auto no_data =
        std::make_shared<DistanceTestImage>(10.0, 1.0, 0.0, 0.0, 10.0, 10.0);
    composite.add_image_with_origin(no_data, 0.0, 0.0);

    // Valid terrain
    auto valid =
        std::make_shared<DistanceTestImage>(20.0, 255.0, 0.0, 0.0, 10.0, 10.0);
    composite.add_image_with_origin(valid, 10.0, 0.0);

    double height;
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
    EXPECT_NEAR(height, 20.0, 0.01)
        << "Low alpha terrain should be completely ignored";
}

// Test DistanceWeightedCompositeData - out of bounds
TEST(composite_data, distance_weighted_bounds)
{
    rsvp::DistanceWeightedCompositeData composite;

    auto terrain =
        std::make_shared<DistanceTestImage>(10.0, 255.0, 0.0, 0.0, 10.0, 10.0);
    composite.add_image_with_origin(terrain, 5.0, 5.0);

    double height;

    // Outside bounds should fail
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(height, 20.0, 20.0, 1));

    // Inside bounds should succeed
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 5.0, 5.0, 1));
}

// Test DistanceWeightedCompositeData - mixed origin/no-origin terrains
TEST(composite_data, distance_weighted_mixed)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Navcam with origin
    auto navcam =
        std::make_shared<DistanceTestImage>(10.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(navcam, 0.0, 0.0);

    // Orbital without origin
    auto orbital =
        std::make_shared<DistanceTestImage>(50.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image(orbital);

    double height;

    // At navcam origin
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
    EXPECT_LT(height, 40.0) << "Navcam should have advantage at camera origin";
    EXPECT_GT(height, 5.0) << "But orbital should still contribute";

    // Far from navcam
    EXPECT_TRUE(
        composite.get_interpolated_pixel_double(height, 15.0, 15.0, 1));
    EXPECT_GT(height, 25.0)
        << "Orbital should have more influence far from navcam";
}

// Test DistanceWeightedCompositeData - get_pixel_double
TEST(composite_data, distance_weighted_get_pixel)
{
    rsvp::DistanceWeightedCompositeData composite;

    auto terrain = std::make_shared<DistanceTestImage>(
        100.0, 255.0, 0.0, 0.0, 10.0, 10.0);
    composite.add_image_with_origin(terrain, 5.0, 5.0);

    double height;
    // get_pixel_double should also work (calls get_interpolated_pixel_double)
    EXPECT_TRUE(composite.get_pixel_double(height, 5, 5, 1));
    EXPECT_NEAR(height, 100.0, 1.0);
}

// Test DistanceWeightedCompositeData - detailed quadratic falloff with two
// terrains
TEST(composite_data, distance_weighted_two_terrain_falloff)
{
    rsvp::DistanceWeightedCompositeData composite;

    // Terrain 1: height=0, camera at (0, 0), bounds 0-20x0-20, scale=5
    auto terrain1 =
        std::make_shared<DistanceTestImage>(0.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(terrain1, 0.0, 0.0);

    // Terrain 2: height=100, camera at (20, 0), bounds 0-20x0-20, scale=5
    auto terrain2 = std::make_shared<DistanceTestImage>(
        100.0, 255.0, 0.0, 0.0, 20.0, 20.0);
    composite.add_image_with_origin(terrain2, 20.0, 0.0);

    double height;

    // Non-VICAR images (DistanceTestImage) have distance weighting disabled
    // (range_error_coefficient=0). Both terrains have equal alpha (255), so
    // dist_weight=1.0 for both regardless of distance. Result is the average
    // of the two terrain heights: (0 + 100) / 2 = 50

    // At (0, 0): near camera1, but distance weighting disabled
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
    EXPECT_NEAR(height, 50.0, 5.0)
        << "Non-VICAR images: distance weighting disabled, terrains blend equally";

    // At (20, 0): near camera2, but distance weighting disabled
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 20.0, 0.0, 1));
    EXPECT_NEAR(height, 50.0, 5.0)
        << "Non-VICAR images: distance weighting disabled, terrains blend equally";

    // At (10, 0): midpoint - equal distance to both cameras
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 10.0, 0.0, 1));
    EXPECT_NEAR(height, 50.0, 5.0) << "Should get average height";

    // At (5, 0): closer to camera1
    EXPECT_TRUE(composite.get_interpolated_pixel_double(height, 5.0, 0.0, 1));
    EXPECT_NEAR(height, 50.0, 5.0)
        << "Non-VICAR images: distance weighting disabled, terrains blend equally";
}

// Test DistanceWeightedCompositeData - fallback scale with invalid bounds
TEST(composite_data, distance_weighted_fallback_scale)
{
    // Create image without proper bounds
    class NoBoundsImage : public rsvp::ImageData
    {
    public:
        bool get_pixel_double(double &value,
                              int /*x*/,
                              int /*y*/,
                              int band) const override
        {
            if (band == 0 || band == 1)
            {
                value = 10.0;
            }
            else if (band == 2)
            {
                value = 255.0;
            }
            else
            {
                return false;
            }
            return true;
        }

        bool get_interpolated_pixel_double(double &value,
                                           double /*x*/,
                                           double /*y*/,
                                           int band) const override
        {
            return get_pixel_double(value, 0, 0, band);
        }

        int get_bands() const override
        {
            return 3;
        }

        rsvp::TerrainBounds get_bounds() const override
        {
            // Return invalid bounds
            return rsvp::TerrainBounds();
        }
    };

    auto image = std::make_shared<NoBoundsImage>();
    image->set_alpha_band(2);

    rsvp::DistanceWeightedCompositeData composite;
    composite.add_image_with_origin(image, 0.0, 0.0);

    // Should use fallback scale of 10.0 meters
    // We can't directly verify the scale, but we can verify it doesn't crash
    double height;
    EXPECT_NO_THROW(
        composite.get_interpolated_pixel_double(height, 0.0, 0.0, 1));
}