#include <image_data.h>
#include <platform.h>
#include <z_offset_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

// Test the inline methods in image_data.h
TEST(inline_methods, image_data_defaults)
{
    // Create a test class that inherits from ImageData but doesn't override
    // the inline methods
    class TestImageData : public rsvp::ImageData
    {
    public:
        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            return false;
        }
        int get_bands() const override
        {
            return 1;
        }
    };

    TestImageData img;

    // Test the default implementations of get_width and get_height
    EXPECT_EQ(img.get_width(), 0);
    EXPECT_EQ(img.get_height(), 0);
}

// Test the inline methods in z_offset_data.h
TEST(inline_methods, z_offset_data)
{
    // Test with null image
    rsvp::ZOffsetData nullData(nullptr);
    EXPECT_EQ(nullData.get_width(), 0);
    EXPECT_EQ(nullData.get_height(), 0);

    // Create a simple test image with width = 3, height = 4
    class TestImageData : public rsvp::ImageData
    {
    public:
        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            return false;
        }
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
            return 4;
        }
    };

    auto testImg = std::make_shared<TestImageData>();
    rsvp::ZOffsetData offsetData(testImg);

    // Test that the get_width and get_height methods pass through to the
    // underlying image
    EXPECT_EQ(offsetData.get_width(), 3);
    EXPECT_EQ(offsetData.get_height(), 4);
}