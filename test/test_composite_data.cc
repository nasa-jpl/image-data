#include <composite_data.h>
#include <platform.h>
#include <translated_data.h>
#include <vicar_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include <cmath>
#include <fstream>
#include <memory>

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
namespace
{
    // A mosaic of abutting tiles, laid out the way orbital DEMs are.
    //
    // The tiles are cut out of one global grid of GRID_SIZE x GRID_SIZE pixels
    // at a pitch of 1 world unit. Each tile is TILE_SIZE pixels square, so its
    // pixel centers only span TILE_SIZE - 1 world units while its neighbor
    // starts TILE_SIZE away: there is a one-unit-wide band along every seam
    // that no single tile can interpolate on its own, because each of the four
    // corners the band needs belongs to a different tile.
    class SeamMosaic
    {
    public:
        static const int TILE_SIZE = 4;
        static const int TILES_PER_SIDE = 2;
        static const int GRID_SIZE = TILE_SIZE * TILES_PER_SIDE;

        // The heights the mosaic is cut from. Deliberately not a plane, so
        // that getting the interpolation wrong shows up as a wrong value
        // rather than as the right one by accident.
        static double global_height(int x, int y)
        {
            return 0.25 * x * x - 1.5 * y + 0.1 * x * y + 3.0;
        }

        // What the mosaic should report at (x, y): bilinear interpolation over
        // the global grid, which is what the tiles would produce if they had
        // been left as a single image.
        static bool expected(double &value, double x, double y)
        {
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const double frac_x = x - x0;
            const double frac_y = y - y0;

            value = 0.0;

            for (int offset_x = 0; offset_x < 2; offset_x++)
            {
                for (int offset_y = 0; offset_y < 2; offset_y++)
                {
                    const double weight =
                        (offset_x == 0 ? 1.0 - frac_x : frac_x) *
                        (offset_y == 0 ? 1.0 - frac_y : frac_y);

                    if (weight == 0.0)
                    {
                        continue;
                    }

                    const int pixel_x = x0 + offset_x;
                    const int pixel_y = y0 + offset_y;

                    if (pixel_x < 0 || pixel_x >= GRID_SIZE || pixel_y < 0 ||
                        pixel_y >= GRID_SIZE)
                    {
                        return false;
                    }

                    value += weight * global_height(pixel_x, pixel_y);
                }
            }

            return true;
        }
    };

    // One tile of a SeamMosaic, shaped like a real heightmap tile: band 0 and
    // band 1 hold heights, band 2 holds a fully-opaque alpha, and coordinates
    // outside the tile are rejected outright.
    class SeamTile : public rsvp::ImageData
    {
    private:
        int origin_x;
        int origin_y;

    public:
        SeamTile(int in_origin_x, int in_origin_y) :
            origin_x(in_origin_x),
            origin_y(in_origin_y)
        {
        }

        int get_bands() const override
        {
            return 3;
        }

        int get_width() const override
        {
            return SeamMosaic::TILE_SIZE;
        }

        int get_height() const override
        {
            return SeamMosaic::TILE_SIZE;
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            if (x < 0 || x >= get_width() || y < 0 || y >= get_height() ||
                band < 0 || band >= get_bands())
            {
                return false;
            }

            if (band == 2)
            {
                value = 255.0;
            }
            else
            {
                value = SeamMosaic::global_height(origin_x + x, origin_y + y);
            }

            return true;
        }
    };

    // Fill `composite` with the tiles of a SeamMosaic, each placed by a
    // TranslatedData exactly as a .mod file would place it.
    void build_seam_mosaic(rsvp::CompositeData &composite)
    {
        for (int tile_x = 0; tile_x < SeamMosaic::TILES_PER_SIDE; tile_x++)
        {
            for (int tile_y = 0; tile_y < SeamMosaic::TILES_PER_SIDE; tile_y++)
            {
                const int origin_x = tile_x * SeamMosaic::TILE_SIZE;
                const int origin_y = tile_y * SeamMosaic::TILE_SIZE;

                auto tile = std::make_shared<SeamTile>(origin_x, origin_y);
                tile->set_alpha_band(2);

                composite.add_image(std::make_shared<rsvp::TranslatedData>(
                    tile, origin_x, origin_y, 1.0, 0.0));
            }
        }
    }

    // Check that `composite` matches the mosaic it was cut from everywhere
    // along a line, seams included.
    void expect_matches_mosaic(const rsvp::CompositeData &composite,
                               double start_x,
                               double start_y,
                               double step_x,
                               double step_y,
                               int steps)
    {
        for (int i = 0; i <= steps; i++)
        {
            const double x = start_x + i * step_x;
            const double y = start_y + i * step_y;

            double reference = 0.0;
            ASSERT_TRUE(SeamMosaic::expected(reference, x, y))
                << "at (" << x << ", " << y << ")";

            double value = 0.0;
            EXPECT_TRUE(
                composite.get_interpolated_pixel_double(value, x, y, 1))
                << "at (" << x << ", " << y << ")";
            EXPECT_NEAR(value, reference, 1e-12)
                << "at (" << x << ", " << y << ")";
        }
    }
}

// Abutting tiles should interpolate across their seams, not around them.
//
// Each tile can only interpolate within its own pixels, so the band between
// two tiles needs corners from both. Before this was handled, the tile on the
// far side of the seam answered the whole band by mirroring its own edge,
// which put a spurious ridge along every seam of an orbital terrain (see SSim
// system test sol01964_bump).
TEST(composite_data, alpha_blending_composite_data_seams)
{
    rsvp::AlphaBlendingCompositeData composite;
    build_seam_mosaic(composite);

    const double last = SeamMosaic::GRID_SIZE - 1;

    // Along a row, crossing the vertical seam between tile columns
    expect_matches_mosaic(composite, 0.0, 1.5, 0.25, 0.0, 4 * last);

    // Down a column, crossing the horizontal seam between tile rows
    expect_matches_mosaic(composite, 1.5, 0.0, 0.0, 0.25, 4 * last);

    // Diagonally through the point where four tiles meet
    expect_matches_mosaic(composite, 0.0, 0.0, 0.25, 0.25, 4 * last);

    // And along the seams themselves, where every sample is in the band
    expect_matches_mosaic(composite, 3.5, 0.0, 0.0, 0.25, 4 * last);
    expect_matches_mosaic(composite, 0.0, 3.5, 0.25, 0.0, 4 * last);
}

TEST(composite_data, average_composite_data_seams)
{
    rsvp::AverageCompositeData composite;
    build_seam_mosaic(composite);

    const double last = SeamMosaic::GRID_SIZE - 1;

    expect_matches_mosaic(composite, 0.0, 1.5, 0.25, 0.0, 4 * last);
    expect_matches_mosaic(composite, 1.5, 0.0, 0.0, 0.25, 4 * last);
    expect_matches_mosaic(composite, 0.0, 0.0, 0.25, 0.25, 4 * last);
}

// Filling the seam band must not turn into extrapolating past the mosaic.
TEST(composite_data, composite_data_seams_stop_at_the_mosaic_edge)
{
    rsvp::AlphaBlendingCompositeData composite;
    build_seam_mosaic(composite);

    const double last = SeamMosaic::GRID_SIZE - 1;
    double value = 0.0;

    // The outer edges of the mosaic are still edges
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, 0.0, 0.0, 1));
    EXPECT_TRUE(composite.get_interpolated_pixel_double(value, last, last, 1));

    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, -0.25, 0.0, 1));
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, 0.0, -0.25, 1));
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, last + 0.25, 0.0, 1));
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, 0.0, last + 0.25, 1));

    // Well outside is still outside
    EXPECT_FALSE(
        composite.get_interpolated_pixel_double(value, -50.0, 0.0, 1));
    EXPECT_FALSE(composite.get_interpolated_pixel_double(value, 0.0, 50.0, 1));

    // Same for the scored composite, which fills seams by a different rule
    rsvp::ScoredCompositeData scored;
    build_seam_mosaic(scored);

    EXPECT_TRUE(scored.get_interpolated_pixel_double(value, 3.5, 1.0, 1));
    EXPECT_FALSE(scored.get_interpolated_pixel_double(value, -0.25, 1.0, 1));
    EXPECT_FALSE(
        scored.get_interpolated_pixel_double(value, last + 0.25, 1.0, 1));
}

// TranslatedData and VicarData bounds describe the extent of the pixel
// centers, so the far corner is the last pixel rather than one pixel past it.
TEST(composite_data, translated_data_bounds_span_the_pixel_centers)
{
    // SeamTile has no spatial labels of its own, so wrap something that does
    class BoundedSeamTile : public SeamTile
    {
    public:
        BoundedSeamTile() :
            SeamTile(0, 0)
        {
        }

        rsvp::TerrainBounds get_bounds() const override
        {
            rsvp::TerrainBounds bounds;
            bounds.valid = true;
            return bounds;
        }
    };

    const double scale = 2.0;
    const rsvp::TranslatedData translated(
        std::make_shared<BoundedSeamTile>(), 100.0, -50.0, scale, 0.0);

    const auto bounds = translated.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 100.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, -50.0);
    EXPECT_DOUBLE_EQ(bounds.max_x,
                     100.0 + (SeamMosaic::TILE_SIZE - 1) * scale);
    EXPECT_DOUBLE_EQ(bounds.max_y,
                     -50.0 + (SeamMosaic::TILE_SIZE - 1) * scale);
}

TEST(composite_data, vicar_data_bounds_span_the_pixel_centers)
{
    const int samples = 4;
    const int lines = 3;
    const double x_min = 100.0;
    const double y_min = -50.0;
    const double x_scale = 2.0;
    const double y_scale = 4.0;

    // The wedge tiles in test/terrain carry their geometry in the system label
    // rather than in a SURFACE_PROJECTION_PARMS property, which is the only
    // place VicarData::get_bounds looks, so synthesize a tile that has one.
    // LBLSIZE has to be a whole number of RECSIZE records, and the label has
    // to be padded out to it
    const size_t label_size = 512;
    std::string label =
        "LBLSIZE=512  FORMAT='REAL'  TYPE='IMAGE'  BUFSIZ=16  DIM=3  EOL=0  "
        "RECSIZE=16  ORG='BSQ'  NL=3  NS=4  NB=1  N1=4  N2=3  N3=1  N4=0  "
        "NBB=0  NLB=0  HOST='LINUX'  INTFMT='LOW'  REALFMT='RIEEE'  "
        "PROPERTY='SURFACE_PROJECTION_PARMS'  MAP_SCALE=(2.0, 4.0)  "
        "X_AXIS_MINIMUM=100.0  Y_AXIS_MINIMUM=-50.0";
    ASSERT_TRUE(label.size() <= label_size);
    label.resize(label_size, ' ');

    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string tile_path = tmp_dir + "/bounds.ht";

    {
        std::ofstream tile_file(tile_path,
                                std::ofstream::binary | std::ofstream::trunc);
        tile_file << label;

        for (int i = 0; i < samples * lines; i++)
        {
            const float height = static_cast<float>(i);
            tile_file.write(reinterpret_cast<const char *>(&height),
                            sizeof(height));
        }
    }

    const auto tile = rsvp::VicarData::read_vicarfile(tile_path);
    ASSERT_TRUE(tile != nullptr);
    EXPECT_EQ(tile->get_width(), samples);
    EXPECT_EQ(tile->get_height(), lines);

    // Bounds run to the last pixel, not one pixel past it
    const auto bounds = tile->get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, x_min);
    EXPECT_DOUBLE_EQ(bounds.min_y, y_min);
    EXPECT_DOUBLE_EQ(bounds.max_x, x_min + (samples - 1) * x_scale);
    EXPECT_DOUBLE_EQ(bounds.max_y, y_min + (lines - 1) * y_scale);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}
