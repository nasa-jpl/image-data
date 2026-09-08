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

TEST(image_data_interpolation, get_interpolated_pixel_double_negative_fraction)
{
    InterpolationTestImage img;
    double value = 0.0;

    // A coordinate between -1 and 0 is outside the image: interpolating it
    // would need the pixel at -1. Truncating toward zero used to leave the
    // lower corner at 0 and mirror the interpolation about the edge, quietly
    // returning the value from +0.75 and reporting success. That reflection is
    // what put a bump along every seam of a tiled terrain.
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, -0.75, 0.0, 0));
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, 0.0, -0.75, 0));
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, -0.25, -0.25, 0));

    // Well outside the image is still outside the image
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, -1.5, 0.0, 0));
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, 0.0, -1.5, 0));
}

TEST(image_data_interpolation, get_interpolated_pixel_double_last_pixel)
{
    InterpolationTestImage img;
    double value = 0.0;

    // Landing exactly on the last row or column needs no neighbor to the right
    // or below, so it should not be rejected for lacking one.
    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 2.0, 2.0, 0));
    EXPECT_NEAR(value, 9.0, 0.001);

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 2.0, 0.5, 0));
    EXPECT_NEAR(value, 4.5, 0.001); // Average of (2,0) = 3 and (2,1) = 6

    EXPECT_TRUE(img.get_interpolated_pixel_double(value, 0.5, 2.0, 0));
    EXPECT_NEAR(value, 7.5, 0.001); // Average of (0,2) = 7 and (1,2) = 8

    // But any fraction past the last pixel is genuinely out of bounds
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, 2.25, 2.0, 0));
    EXPECT_FALSE(img.get_interpolated_pixel_double(value, 2.0, 2.25, 0));
}

TEST(image_data_interpolation, get_clamped_pixel_double)
{
    InterpolationTestImage img;
    double value = 0.0;
    double weight = 0.0;

    // Inside the image, nothing is clamped and the sample carries full weight
    EXPECT_TRUE(img.get_clamped_pixel_double(value, weight, 0.5, 0.5, 0));
    EXPECT_NEAR(value, 3.0, 0.001);
    EXPECT_NEAR(weight, 1.0, 0.001);

    // A quarter pixel off the left edge clamps to the edge and keeps 3/4 of a
    // say in the answer
    EXPECT_TRUE(img.get_clamped_pixel_double(value, weight, -0.25, 1.0, 0));
    EXPECT_NEAR(value, 4.0, 0.001); // Pixel (0, 1)
    EXPECT_NEAR(weight, 0.75, 0.001);

    // Same past the far edge, which is at width - 1
    EXPECT_TRUE(img.get_clamped_pixel_double(value, weight, 2.25, 1.0, 0));
    EXPECT_NEAR(value, 6.0, 0.001); // Pixel (2, 1)
    EXPECT_NEAR(weight, 0.75, 0.001);

    // Off the corner, the weights of both axes apply
    EXPECT_TRUE(img.get_clamped_pixel_double(value, weight, -0.5, -0.25, 0));
    EXPECT_NEAR(value, 1.0, 0.001); // Pixel (0, 0)
    EXPECT_NEAR(weight, 0.375, 0.001);

    // A full pixel out or further, and the image has no say at all
    EXPECT_FALSE(img.get_clamped_pixel_double(value, weight, -1.0, 1.0, 0));
    EXPECT_FALSE(img.get_clamped_pixel_double(value, weight, 3.0, 1.0, 0));
    EXPECT_FALSE(img.get_clamped_pixel_double(value, weight, -2.5, 1.0, 0));
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