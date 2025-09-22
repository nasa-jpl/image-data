#include <image_data.h>
#include <pgm_data.h>
#include <platform.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

class InterpolationTestImage : public rsvp::ImageData
{
private:
    double data[9] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    int width = 3;
    int height = 3;

public:
    InterpolationTestImage() = default;

    int get_bands() const override
    {
        return 1;
    }

    bool get_pixel_double(double &value, int x, int y, int band) const override
    {
        if (x < 0 || x >= width || y < 0 || y >= height || band != 0)
        {
            return false;
        }
        value = data[y * width + x];
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

TEST(image_data_interpolation, interpolation_enabled_disabled)
{
    InterpolationTestImage img;

    // Test interpolation is enabled by default
    EXPECT_EQ(img.get_interpolating(), true);

    // Test setting interpolation to disabled
    img.set_interpolating(false);
    EXPECT_EQ(img.get_interpolating(), false);

    // Test setting it back to enabled
    img.set_interpolating(true);
    EXPECT_EQ(img.get_interpolating(), true);
}

TEST(image_data_interpolation, get_interpolated_pixel_double_disabled)
{
    InterpolationTestImage img;
    img.set_interpolating(false);

    double value;

    // When interpolation is disabled, it should snap to nearest pixel
    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 0.7, 0.7, 0));
    EXPECT_NEAR(value, 5.0, 0.001); // Should snap to (1,1)

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 0.2, 0.2, 0));
    EXPECT_NEAR(value, 1.0, 0.001); // Should snap to (0,0)

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 1.7, 1.7, 0));
    EXPECT_NEAR(value, 9.0, 0.001); // Should snap to (2,2)
}

TEST(image_data_interpolation, get_interpolated_pixel_double_enabled)
{
    InterpolationTestImage img;
    double value;

    // When interpolation is enabled, it should interpolate between pixels
    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 0.5, 0.5, 0));
    EXPECT_NEAR(
        value, 3.0, 0.001); // Should be average of (0,0), (1,0), (0,1), (1,1)

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 1.0, 1.0, 0));
    EXPECT_NEAR(value, 5.0, 0.001); // Should be exactly (1,1)

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 1.5, 1.5, 0));
    EXPECT_NEAR(
        value, 7.0, 0.001); // Should be average of (1,1), (2,1), (1,2), (2,2)
}

TEST(image_data_interpolation, get_interpolated_pixel_double_out_of_bounds)
{
    InterpolationTestImage img;
    double value;

    // Test out of bounds
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, 3.0, 3.0, 0));
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, -1.0, -1.0, 0));
}

TEST(image_data_interpolation, get_pixel_int)
{
    InterpolationTestImage img;
    int value;

    // Test normal case
    EXPECT_TRUE(img.get_pixel_int(value, 1, 1, 0));
    EXPECT_EQ(value, 5);

    // Test rounding positive
    img.set_interpolating(true);
    double dvalue;
    EXPECT_TRUE(img.get_interpolated_pixel_double(dvalue, 0.7, 0.7, 0));
    EXPECT_TRUE(img.get_pixel_int(value, 1, 1, 0));

    // Test rounding negative case
    EXPECT_TRUE(img.get_pixel_int(value, 0, 0, 0)); // Gets 1.0
    EXPECT_EQ(value, 1);

    // Test out of bounds
    EXPECT_FALSE(img.get_pixel_int(value, 10, 10, 0));
}

TEST(image_data_interpolation, get_interpolated_pixel_int)
{
    InterpolationTestImage img;
    int value;

    // Test normal case with rounding
    EXPECT_TRUE(img.get_interpolated_pixel_int(value, 0.5, 0.5, 0));
    EXPECT_EQ(value, 3);

    // Test rounding positive value
    EXPECT_TRUE(img.get_interpolated_pixel_int(value, 1.75, 1.75, 0));

    // Test out of bounds
    EXPECT_FALSE(img.get_interpolated_pixel_int(value, 10.5, 10.5, 0));
}

TEST(image_data_interpolation, alpha_band)
{
    InterpolationTestImage img;

    // Test default alpha band
    EXPECT_EQ(img.get_alpha_band(), -1);

    // Test setting alpha band
    img.set_alpha_band(1);
    EXPECT_EQ(img.get_alpha_band(), 1);
}