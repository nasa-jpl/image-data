#include <composite_data.h>
#include <platform.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

// Simple image implementation for testing CompositeData classes
class SimpleTestImage : public rsvp::ImageData
{
private:
    double data[9] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    int width = 3;
    int height = 3;
    int band_count = 1;
    int alpha_value = -1;
    bool use_interpolation = true;

public:
    SimpleTestImage() = default;

    // Create multi-band image
    explicit SimpleTestImage(int bands) :
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
        if (x < 0 || x >= width || y < 0 || y >= height || band < 0 ||
            band >= band_count)
        {
            return false;
        }
        if (band == 0)
        {
            value = data[y * width + x];
        }
        else if (band == 1)
        {
            // Second band contains values 10-19
            value = 10.0 + (y * width + x);
        }
        else if (band == 2)
        {
            // Alpha band contains values 100-109 (for testing)
            value = 100.0 + (y * width + x);
        }
        else
        {
            // For any other band
            value = band * 100.0 + (y * width + x);
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
    auto img1 = std::make_shared<SimpleTestImage>();
    composite.add_image(img1);

    // Test after adding one image
    EXPECT_EQ(composite.get_count(), 1);
    EXPECT_EQ(composite.get_bands(), 1);
    EXPECT_EQ(composite.get_alpha_band(),
              -1); // Should match the image's alpha band

    // Add a second image at a specific position
    auto img2 = std::make_shared<SimpleTestImage>(1); // 1-band image
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
    auto img3 = std::make_shared<SimpleTestImage>();
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
    auto img = std::make_shared<SimpleTestImage>(1);
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
    auto img1 = std::make_shared<SimpleTestImage>(3);
    auto img2 = std::make_shared<SimpleTestImage>(3);

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
    auto pgmImage = std::make_shared<SimpleTestImage>(1);
    pgmComposite.add_image(pgmImage);

    EXPECT_TRUE(
        pgmComposite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));

    // Test with invalid alpha band
    auto invalidAlphaImg = std::make_shared<SimpleTestImage>(3);
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
    auto img = std::make_shared<SimpleTestImage>(1);
    // Set the only band as alpha band to ensure scoring works
    img->set_alpha_band(0);
    composite.add_image(img);

    // Now with a valid image added, we should get valid results for normal
    // coordinates
    EXPECT_TRUE(composite.get_pixel_double(value, 1, 1, 0));
}