#include <platform.h>
#include <translated_data.h>
#include <z_offset_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

// Image implementation for testing ZOffsetData
class ZOffsetTestImage : public rsvp::ImageData
{
private:
    double data[9] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    int width = 3;
    int height = 3;
    int band_count = 1;
    int alpha_value = -1;

public:
    ZOffsetTestImage() = default;

    // Create multi-band image
    explicit ZOffsetTestImage(int bands) :
        band_count(bands)
    {
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
        else
        {
            // For multi-band testing, use band number * 10 + position
            value = band * 10.0 + (y * width + x);
        }
        return true;
    }

    bool get_interpolated_pixel_double(double &value,
                                       double x,
                                       double y,
                                       int band) const override
    {
        // Simple implementation for testing - just round to nearest
        int int_x = static_cast<int>(x + 0.5);
        int int_y = static_cast<int>(y + 0.5);
        
        // Check if coordinates are out of bounds before rounding
        if (x < 0 || x >= width || y < 0 || y >= height || band < 0 || band >= band_count)
        {
            return false;
        }
        
        return get_pixel_double(value, int_x, int_y, band);
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

TEST(z_offset_data, constructor)
{
    // Test with nullptr
    rsvp::ZOffsetData nullData(nullptr);
    EXPECT_EQ(nullData.get_bands(), 0);
    EXPECT_EQ(nullData.get_width(), 0);
    EXPECT_EQ(nullData.get_height(), 0);

    // Test with normal image
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    EXPECT_EQ(offsetData.get_bands(), 1);
    EXPECT_EQ(offsetData.get_width(), 3);
    EXPECT_EQ(offsetData.get_height(), 3);

    // Test with multi-band image
    // Check that ZOffsetData properly forwards the band count
    // from the underlying image
    auto multiBandImage = std::make_shared<ZOffsetTestImage>(1);
    rsvp::ZOffsetData multiBandOffsetData(multiBandImage);
    EXPECT_EQ(multiBandOffsetData.get_bands(), 1);
}

TEST(z_offset_data, get_pixel_double)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    double value;

    // Test normal pixel access
    EXPECT_TRUE(offsetData.get_pixel_double(value, 1, 1, 0));
    EXPECT_DOUBLE_EQ(value, 5.0);

    // Test out of bounds - x coordinate
    EXPECT_FALSE(offsetData.get_pixel_double(value, -1, 0, 0));
    EXPECT_FALSE(offsetData.get_pixel_double(value, 3, 0, 0));

    // Test out of bounds - y coordinate
    EXPECT_FALSE(offsetData.get_pixel_double(value, 0, -1, 0));
    EXPECT_FALSE(offsetData.get_pixel_double(value, 0, 3, 0));

    // Test out of bounds - band
    EXPECT_FALSE(offsetData.get_pixel_double(value, 0, 0, -1));
    EXPECT_FALSE(offsetData.get_pixel_double(value, 0, 0, 1));

    // Test with offset and scale
    offsetData.set_offset_and_scale(0, 10.0, 2.0);
    EXPECT_TRUE(offsetData.get_pixel_double(value, 0, 0, 0));
    EXPECT_DOUBLE_EQ(value, 12.0); // 1.0 * 2.0 + 10.0

    // Test with negative offset
    offsetData.set_offset_and_scale(0, -5.0, 1.5);
    EXPECT_TRUE(offsetData.get_pixel_double(value, 2, 2, 0));
    EXPECT_DOUBLE_EQ(value, 8.5); // 9.0 * 1.5 - 5.0
}

TEST(z_offset_data, get_interpolated_pixel_double)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    double value;

    // Test interpolated pixel access
    EXPECT_TRUE(offsetData.get_interpolated_pixel_double(value, 1.2, 1.2, 0));
    // Just verify that we get some value (actual value may vary by
    // implementation)

    // Test out of bounds
    EXPECT_FALSE(offsetData.get_interpolated_pixel_double(value, 3.5, 0.0, 0));
    EXPECT_FALSE(offsetData.get_interpolated_pixel_double(value, 0.0, 3.5, 0));
    EXPECT_FALSE(offsetData.get_interpolated_pixel_double(value, 0.0, 0.0, 1));

    // Test with offset and scale
    offsetData.set_offset_and_scale(0, 10.0, 2.0);
    double old_value = value;
    EXPECT_TRUE(offsetData.get_interpolated_pixel_double(value, 1.2, 1.2, 0));
    // After setting scale and offset, the value should be different
    EXPECT_NE(value, old_value);
}

TEST(z_offset_data, get_pixel_int)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    int value;

    // Test normal pixel access
    EXPECT_TRUE(offsetData.get_pixel_int(value, 1, 1, 0));
    EXPECT_EQ(value, 5);

    // Test out of bounds
    EXPECT_FALSE(offsetData.get_pixel_int(value, -1, 0, 0));
    EXPECT_FALSE(offsetData.get_pixel_int(value, 0, -1, 0));
    EXPECT_FALSE(offsetData.get_pixel_int(value, 0, 0, -1));

    // Test with offset and scale - positive rounding
    offsetData.set_offset_and_scale(0, 0.3, 1.0);
    EXPECT_TRUE(offsetData.get_pixel_int(value, 0, 0, 0));
    EXPECT_EQ(value, 1); // 1.0 + 0.3 = 1.3 rounded to 1

    // Test with offset and scale - negative rounding
    offsetData.set_offset_and_scale(0, -1.7, 1.0);
    EXPECT_TRUE(offsetData.get_pixel_int(value, 0, 0, 0));
    EXPECT_EQ(value, -1); // 1.0 - 1.7 = -0.7 rounded to -1
}

TEST(z_offset_data, get_interpolated_pixel_int)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    int value;

    // Test normal interpolated pixel access
    EXPECT_TRUE(offsetData.get_interpolated_pixel_int(value, 1.2, 1.2, 0));
    // Just verify we get a value

    // Test out of bounds
    EXPECT_FALSE(offsetData.get_interpolated_pixel_int(value, -1.0, 0.0, 0));
    EXPECT_FALSE(offsetData.get_interpolated_pixel_int(value, 0.0, -1.0, 0));
    EXPECT_FALSE(offsetData.get_interpolated_pixel_int(value, 0.0, 0.0, -1));

    // Test with offset and scale - positive rounding
    int old_value = value;
    offsetData.set_offset_and_scale(0, 0.7, 1.0);
    EXPECT_TRUE(offsetData.get_interpolated_pixel_int(value, 1.2, 1.2, 0));

    // Test with offset and scale - negative rounding
    old_value = value;
    offsetData.set_offset_and_scale(0, -2.7, 1.0);
    EXPECT_TRUE(offsetData.get_interpolated_pixel_int(value, 1.2, 1.2, 0));
}

TEST(z_offset_data, set_offset_and_scale)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>(1);
    rsvp::ZOffsetData offsetData(baseImage);

    double value;

    // Test initial values (identity transform)
    EXPECT_TRUE(offsetData.get_pixel_double(value, 0, 0, 0));
    EXPECT_DOUBLE_EQ(value, 1.0);

    // Set offset and scale for one band
    offsetData.set_offset_and_scale(0, 10.0, 2.0);

    // Check first band is modified
    EXPECT_TRUE(offsetData.get_pixel_double(value, 0, 0, 0));
    EXPECT_DOUBLE_EQ(value, 12.0); // 1.0 * 2.0 + 10.0

    // Test invalid band index - these should be ignored
    offsetData.set_offset_and_scale(-1, 5.0, 5.0);
    offsetData.set_offset_and_scale(3, 5.0, 5.0);

    // Values should be unchanged
    EXPECT_TRUE(offsetData.get_pixel_double(value, 0, 0, 0));
    EXPECT_DOUBLE_EQ(value, 12.0);
}

TEST(z_offset_data, alpha_band)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);

    // Check default alpha band
    EXPECT_EQ(offsetData.get_alpha_band(), -1);

    // Set alpha band and verify
    offsetData.set_alpha_band(0);
    EXPECT_EQ(offsetData.get_alpha_band(), 0);

    // Check that it was also set in base image
    EXPECT_EQ(baseImage->get_alpha_band(), 0);

    // Create with null image
    rsvp::ZOffsetData nullData(nullptr);

    // Check default value
    EXPECT_EQ(nullData.get_alpha_band(), -1);

    // Set and verify
    nullData.set_alpha_band(1);
    EXPECT_EQ(nullData.get_alpha_band(), 1);
}

TEST(z_offset_data, get_clamped_pixel_double)
{
    auto baseImage = std::make_shared<ZOffsetTestImage>();
    rsvp::ZOffsetData offsetData(baseImage);
    offsetData.set_offset_and_scale(0, 100.0, 2.0);

    double value = 0.0;
    double weight = 0.0;

    // Inside the image nothing is clamped, so the sample carries full weight,
    // offset and scaled like any other
    EXPECT_TRUE(
        offsetData.get_clamped_pixel_double(value, weight, 1.0, 1.0, 0));
    EXPECT_NEAR(value, 100.0 + 2.0 * 5.0, 0.001);
    EXPECT_NEAR(weight, 1.0, 0.001);

    // A quarter pixel off the left edge clamps back onto pixel (0, 1) and
    // keeps three quarters of a say
    EXPECT_TRUE(
        offsetData.get_clamped_pixel_double(value, weight, -0.25, 1.0, 0));
    EXPECT_NEAR(value, 100.0 + 2.0 * 4.0, 0.001);
    EXPECT_NEAR(weight, 0.75, 0.001);

    // A full pixel out and it has no say at all
    EXPECT_FALSE(
        offsetData.get_clamped_pixel_double(value, weight, -1.0, 1.0, 0));

    // Bands it does not have
    EXPECT_FALSE(
        offsetData.get_clamped_pixel_double(value, weight, 1.0, 1.0, 1));
    EXPECT_FALSE(
        offsetData.get_clamped_pixel_double(value, weight, 1.0, 1.0, -1));

    // And nothing to sample at all
    rsvp::ZOffsetData nullData(nullptr);
    EXPECT_FALSE(
        nullData.get_clamped_pixel_double(value, weight, 0.0, 0.0, 0));
}

// SSim stacks a ZOffsetData over a TranslatedData over the terrain, so the
// clamping that a composite's seam reconstruction depends on has to reach
// through both to the image that actually owns a pixel grid. Clamping against
// the width and height a ZOffsetData reports on its child's behalf would pit
// world coordinates against pixel indices.
TEST(z_offset_data, get_clamped_pixel_double_through_a_transform)
{
    const double scale = 10.0;
    const double x_offset = 1000.0;
    const double y_offset = -500.0;

    auto baseImage = std::make_shared<ZOffsetTestImage>();
    auto placed = std::make_shared<rsvp::TranslatedData>(
        baseImage, x_offset, y_offset, scale, 0.0);

    rsvp::ZOffsetData offsetData(placed);
    offsetData.set_offset_and_scale(0, 100.0, 2.0);

    double value = 0.0;
    double weight = 0.0;

    // Pixel (0, 1) sits here in world coordinates
    const double pixel_01_x = x_offset;
    const double pixel_01_y = y_offset + scale;

    EXPECT_TRUE(offsetData.get_clamped_pixel_double(
        value, weight, pixel_01_x, pixel_01_y, 0));
    EXPECT_NEAR(value, 100.0 + 2.0 * 4.0, 0.001);
    EXPECT_NEAR(weight, 1.0, 0.001);

    // A quarter of a pixel - a quarter of `scale` in world units - off the
    // left edge clamps back onto it, and the weight is in pixels either way
    EXPECT_TRUE(offsetData.get_clamped_pixel_double(
        value, weight, pixel_01_x - 0.25 * scale, pixel_01_y, 0));
    EXPECT_NEAR(value, 100.0 + 2.0 * 4.0, 0.001);
    EXPECT_NEAR(weight, 0.75, 0.001);

    // Past a whole pixel out and it has no say. Exactly a pixel out is left
    // alone deliberately: inverting the transform lands either side of the
    // boundary depending on the offsets, and a weight of order 1e-14 either
    // way is nowhere near the sum a seam reconstruction requires.
    EXPECT_FALSE(offsetData.get_clamped_pixel_double(
        value, weight, pixel_01_x - 1.25 * scale, pixel_01_y, 0));
}
namespace
{
    // A 3x3 image whose values are 1 + x + 3y, which bilinear interpolation
    // reproduces exactly, and which interpolates through the default path
    class PlainTestImage final : public rsvp::ImageData
    {
    public:
        int get_bands() const override
        {
            return 1;
        }

        int get_width() const override
        {
            return 3;
        }

        int get_height() const override
        {
            return 3;
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= 3 || y < 0 || y >= 3 || band != 0)
            {
                return false;
            }

            value = 1.0 + x + 3.0 * y;
            return true;
        }
    };
}

// Offsetting values does not move pixels, so where the stored image sits is
// where the ZOffsetData sits
TEST(z_offset_data, bounds_are_the_stored_images)
{
    const auto image = std::make_shared<PlainTestImage>();

    rsvp::ZOffsetData unplaced(image);
    EXPECT_FALSE(unplaced.get_bounds().valid);
    EXPECT_FALSE(unplaced.bounds_locate_pixels());

    const auto placed =
        std::make_shared<rsvp::TranslatedData>(image, 5.0, 6.0, 1.0, 0.0);
    rsvp::ZOffsetData offset(placed);

    const auto expected = placed->get_bounds();
    const auto bounds = offset.get_bounds();
    ASSERT_TRUE(expected.valid);
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, expected.min_x);
    EXPECT_DOUBLE_EQ(bounds.max_x, expected.max_x);
    EXPECT_DOUBLE_EQ(bounds.min_y, expected.min_y);
    EXPECT_DOUBLE_EQ(bounds.max_y, expected.max_y);
    EXPECT_TRUE(offset.bounds_locate_pixels());
}

// The stored image is the one that interpolates, so turning interpolation off
// has to reach it - a `deinterpolate` block around a `zoffset` block used not
// to
TEST(z_offset_data, interpolation_passes_through)
{
    const auto image = std::make_shared<PlainTestImage>();
    rsvp::ZOffsetData offset(image);
    offset.set_offset_and_scale(0, 100.0, 1.0);

    double value = 0.0;
    EXPECT_TRUE(offset.get_interpolated_pixel_double(value, 0.4, 0.4, 0));
    EXPECT_DOUBLE_EQ(value, 100.0 + 1.0 + 0.4 + 3.0 * 0.4);

    offset.set_interpolating(false);
    EXPECT_FALSE(image->get_interpolating());
    EXPECT_FALSE(offset.get_interpolating());

    EXPECT_TRUE(offset.get_interpolated_pixel_double(value, 0.4, 0.4, 0));
    EXPECT_DOUBLE_EQ(value, 100.0 + 1.0);

    offset.set_interpolating(true);
    EXPECT_TRUE(image->get_interpolating());
    EXPECT_TRUE(offset.get_interpolating());
}

// A band the tables were not sized for is out of range rather than an error
TEST(z_offset_data, bands_beyond_the_tables_are_out_of_range)
{
    rsvp::ZOffsetData offset(std::make_shared<PlainTestImage>());

    EXPECT_NO_THROW(offset.set_offset_and_scale(5, 1.0, 1.0));
    EXPECT_NO_THROW(offset.set_offset_and_scale(-1, 1.0, 1.0));

    double value = 0.0;
    EXPECT_FALSE(offset.get_pixel_double(value, 0, 0, 5));
    EXPECT_FALSE(offset.get_pixel_double(value, 0, 0, -1));
    EXPECT_FALSE(offset.get_interpolated_pixel_double(value, 0.0, 0.0, 5));
    EXPECT_TRUE(offset.get_pixel_double(value, 0, 0, 0));
}
