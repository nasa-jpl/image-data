#include <composite_data.h>
#include <platform.h>
#include <translated_data.h>
#include <vicar_data.h>
#include <z_offset_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include <atomic>
#include <cmath>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "Config.h"

// Image implementation for testing CompositeData classes
class CompositeTestImage : public rsvp::ImageData
{
private:
    double data[9] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    int width = 3;
    int height = 3;
    int band_count = 1;
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
    // band 1 hold heights, band 2 holds an alpha, and coordinates outside the
    // tile are rejected outright.
    class SeamTile : public rsvp::ImageData
    {
    private:
        int origin_x;
        int origin_y;
        double alpha;

    public:
        SeamTile(int in_origin_x, int in_origin_y, double in_alpha = 255.0) :
            origin_x(in_origin_x),
            origin_y(in_origin_y),
            alpha(in_alpha)
        {
        }

        int get_bands() const override
        {
            return 3;
        }

        // A tile is looked up by pixel index, so its bounds are its own grid.
        // Whatever places it - a TranslatedData - transforms those.
        rsvp::TerrainBounds get_bounds() const override
        {
            return pixel_grid_bounds();
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
                value = alpha;
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
    //
    // `declare_alpha_band` mirrors the difference between a tile that came
    // through ModData, which labels the alpha band of everything it reads, and
    // one assembled by hand, which leaves it unset.
    void build_seam_mosaic(rsvp::CompositeData &composite,
                           bool declare_alpha_band = true,
                           double alpha = 255.0)
    {
        for (int tile_x = 0; tile_x < SeamMosaic::TILES_PER_SIDE; tile_x++)
        {
            for (int tile_y = 0; tile_y < SeamMosaic::TILES_PER_SIDE; tile_y++)
            {
                const int origin_x = tile_x * SeamMosaic::TILE_SIZE;
                const int origin_y = tile_y * SeamMosaic::TILE_SIZE;

                auto tile =
                    std::make_shared<SeamTile>(origin_x, origin_y, alpha);

                if (declare_alpha_band)
                {
                    tile->set_alpha_band(2);
                }

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
    const double scale = 2.0;
    const rsvp::TranslatedData translated(
        std::make_shared<SeamTile>(0, 0), 100.0, -50.0, scale, 0.0);

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
    // place VicarData::get_map_bounds looks, so synthesize a tile that has
    // one.
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

    // Map bounds run to the last pixel, not one pixel past it
    const auto map_bounds = tile->get_map_bounds();
    ASSERT_TRUE(map_bounds.valid);
    EXPECT_DOUBLE_EQ(map_bounds.min_x, x_min);
    EXPECT_DOUBLE_EQ(map_bounds.min_y, y_min);
    EXPECT_DOUBLE_EQ(map_bounds.max_x, x_min + (samples - 1) * x_scale);
    EXPECT_DOUBLE_EQ(map_bounds.max_y, y_min + (lines - 1) * y_scale);

    // Lookups on the tile itself take pixel indices, so that is where it
    // says its pixels are, whatever its labels say
    const auto bounds = tile->get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 0.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, 0.0);
    EXPECT_DOUBLE_EQ(bounds.max_x, samples - 1);
    EXPECT_DOUBLE_EQ(bounds.max_y, lines - 1);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 1.0);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A composite has no pixel grid of its own, so a TranslatedData placing one
// cannot derive its extent from a width and a height. It has to transform the
// bounds the composite reports, which are already in the coordinates being
// transformed from.
TEST(composite_data, translated_data_bounds_over_a_composite)
{
    auto mosaic = std::make_shared<rsvp::AlphaBlendingCompositeData>();
    build_seam_mosaic(*mosaic);

    const auto mosaic_bounds = mosaic->get_bounds();
    ASSERT_TRUE(mosaic_bounds.valid);
    EXPECT_DOUBLE_EQ(mosaic_bounds.min_x, 0.0);
    EXPECT_DOUBLE_EQ(mosaic_bounds.max_x, SeamMosaic::GRID_SIZE - 1);

    const double scale = 2.0;
    const rsvp::TranslatedData translated(mosaic, 100.0, -50.0, scale, 0.0);

    const auto bounds = translated.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 100.0 + scale * mosaic_bounds.min_x);
    EXPECT_DOUBLE_EQ(bounds.min_y, -50.0 + scale * mosaic_bounds.min_y);
    EXPECT_DOUBLE_EQ(bounds.max_x, 100.0 + scale * mosaic_bounds.max_x);
    EXPECT_DOUBLE_EQ(bounds.max_y, -50.0 + scale * mosaic_bounds.max_y);
}

// Merging the children's bounds is too slow to redo per pixel lookup, so a
// composite remembers them. Moving a child afterwards has to be visible
// through that.
TEST(composite_data, bounds_follow_a_moved_child)
{
    auto placed = std::make_shared<rsvp::TranslatedData>(
        std::make_shared<SeamTile>(0, 0), 0.0, 0.0, 1.0, 0.0);

    rsvp::AlphaBlendingCompositeData composite;
    composite.add_image(placed);

    // Ask once to get the bounds remembered
    auto bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 0.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, 0.0);

    placed->set_trans(100.0, -50.0, 1.0, 0.0);

    bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 100.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, -50.0);
    EXPECT_DOUBLE_EQ(bounds.max_x, 100.0 + SeamMosaic::TILE_SIZE - 1);
    EXPECT_DOUBLE_EQ(bounds.max_y, -50.0 + SeamMosaic::TILE_SIZE - 1);

    // And adding one has to be too
    composite.add_image(std::make_shared<rsvp::TranslatedData>(
        std::make_shared<SeamTile>(0, 0), -10.0, -10.0, 1.0, 0.0));

    bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, -10.0);
    // The moved tile still reaches lower than the added one
    EXPECT_DOUBLE_EQ(bounds.min_y, -50.0);

    // As does taking one away
    composite.remove_image(1);

    bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 100.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, -50.0);
}

// Whether a seam holds real data has to be judged by the same rule as the
// pixels on either side of it. A three-band tile keeps its alpha in band 2
// whether or not it ever said so, and a tile that came from anywhere but
// ModData will not have said so.
TEST(composite_data, undeclared_alpha_band_still_governs_seams)
{
    // 3.5 is on the vertical seam, 1.5 is inside a tile vertically
    const double seam_x = SeamMosaic::TILE_SIZE - 0.5;
    const double seam_y = 1.5;

    double value = 0.0;

    rsvp::AlphaBlendingCompositeData opaque;
    build_seam_mosaic(opaque, false /* declare_alpha_band */, 255.0);

    double reference = 0.0;
    ASSERT_TRUE(SeamMosaic::expected(reference, seam_x, seam_y));
    EXPECT_TRUE(
        opaque.get_interpolated_pixel_double(value, seam_x, seam_y, 1));
    EXPECT_NEAR(value, reference, 1e-12);

    // The minimum alpha means no real data, so there is nothing to
    // reconstruct the band from - not even though the heights are there
    rsvp::AlphaBlendingCompositeData transparent;
    build_seam_mosaic(transparent, false /* declare_alpha_band */, 1.0);

    EXPECT_FALSE(
        transparent.get_interpolated_pixel_double(value, seam_x, seam_y, 1));

    // Which matches what it does away from the seam
    EXPECT_FALSE(
        transparent.get_interpolated_pixel_double(value, 1.5, 1.5, 1));
}

namespace
{
    const int num_sampling_threads = 8;

    // Sample the composite across a row of the mosaic it was cut from, and
    // check every answer against that mosaic.
    //
    // A torn read gives a wrong answer rather than throwing, so the values
    // matter as much as getting through at all.
    void sample_seam_row(const rsvp::CompositeData &composite,
                         std::atomic<int> &thrown,
                         std::atomic<int> &wrong_values)
    {
        const double last = SeamMosaic::GRID_SIZE - 1;
        const double row_y = 1.5;

        try
        {
            // A quarter of a pixel at a time, so that tile interiors, seams
            // and the outer edge all get sampled
            for (double x = 0.0; x <= last; x += 0.25)
            {
                double value = 0.0;
                double reference = 0.0;

                if (!composite.get_interpolated_pixel_double(
                        value, x, row_y, 1) ||
                    !SeamMosaic::expected(reference, x, row_y) ||
                    std::fabs(value - reference) > 1e-12)
                {
                    wrong_values.fetch_add(1);
                }
            }
        }
        catch (const std::exception &)
        {
            thrown.fetch_add(1);
        }
    }

    // A child that sits wherever an atomic says it does.
    //
    // Moving a real image means writing the plain doubles inside its
    // TranslatedData, which races with any lookup already walking into it -
    // whatever the geometry cache does - and a test built on that could not
    // tell a cache bug from that race. This moves on a single atomic store
    // instead, so a test can move it out from under a lookup and hold the
    // cache alone responsible for what comes back.
    //
    // It reports where it sits and nothing else: every sample of it fails, so
    // it changes where this composite's children sit without changing any of
    // the values the composite gives back.
    class RelocatableTile final : public rsvp::ImageData
    {
    private:
        std::atomic<int> origin {0};

    public:
        /// Move the tile, as placing an image does.
        void move_to(int new_origin)
        {
            origin.store(new_origin);
            rsvp::invalidate_geometry();
        }

        int get_bands() const override
        {
            return 3;
        }

        int get_width() const override
        {
            return 2;
        }

        int get_height() const override
        {
            return 2;
        }

        rsvp::TerrainBounds get_bounds() const override
        {
            const double at = origin.load();

            rsvp::TerrainBounds bounds;
            bounds.valid = true;
            bounds.min_x = at;
            bounds.min_y = at;
            bounds.max_x = at + 1;
            bounds.max_y = at + 1;
            return bounds;
        }

        bool get_pixel_double(double & /*value*/,
                              int /*x*/,
                              int /*y*/,
                              int /*band*/) const override
        {
            return false;
        }
    };
}

// A composite remembers what it knows about each child, and works that out on
// the first lookup that needs it rather than up front. Callers sample terrain
// from more than one thread at a time - SSim runs one thread per core inside
// its range-image simulation - so those lookups race with each other, and any
// image anywhere moving invalidates the cache underneath them.
//
// Refilling the cache in place used to leave a reader indexing a vector that
// another thread had just emptied, which surfaced as std::out_of_range from
// the middle of a drive, or as a corrupted heap when two threads reallocated
// it at once.
//
// This covers an invalidation that leaves this composite's own geometry alone,
// which is what almost all of them are: the version counter is global, so
// every composite's cache is invalidated by any image anywhere moving. See
// concurrent_lookups_survive_a_moved_child for one of its own children moving.
TEST(composite_data, concurrent_lookups_survive_invalidation)
{
    rsvp::AlphaBlendingCompositeData composite;
    build_seam_mosaic(composite);

    const int num_rounds = 200;

    std::atomic<int> round {0};
    std::atomic<int> finished {0};
    std::atomic<bool> stopping {false};
    std::atomic<int> thrown {0};
    std::atomic<int> wrong_values {0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_sampling_threads; t++)
    {
        threads.emplace_back([&] {
            int current_round = 0;

            while (true)
            {
                // Wait for the round to open, so that every thread meets the
                // freshly invalidated cache at once
                while (round.load() == current_round && !stopping.load())
                {
                    std::this_thread::yield();
                }

                if (stopping.load())
                {
                    return;
                }

                current_round = round.load();

                sample_seam_row(composite, thrown, wrong_values);

                finished.fetch_add(1);
            }
        });
    }

    for (int r = 1; r <= num_rounds; r++)
    {
        // Stands in for some other image being placed, which invalidates every
        // composite's cache without moving any of this one's children
        rsvp::invalidate_geometry();

        finished.store(0);
        round.store(r);

        while (finished.load() < num_sampling_threads)
        {
            std::this_thread::yield();
        }
    }

    stopping.store(true);
    round.fetch_add(1);

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(thrown.load(), 0);
    EXPECT_EQ(wrong_values.load(), 0);
}

// One of the composite's own children moving, rather than some unrelated
// image, which is the case the cache cannot answer by restamping what it
// already published: it has to work out a new snapshot and point lookups at it
// while other threads are still reading the old one.
//
// The move deliberately lands mid-lookup rather than between rounds. A thread
// that has already loaded the previous snapshot goes on reading it after it
// has been retired, so retiring one must not free it, and the two must not be
// mixed within a single lookup.
TEST(composite_data, concurrent_lookups_survive_a_moved_child)
{
    rsvp::AlphaBlendingCompositeData composite;
    build_seam_mosaic(composite);

    // Somewhere no sample lands, so that where it sits is all it contributes
    const int stray_origin = 1000;

    const auto stray = std::make_shared<RelocatableTile>();
    stray->move_to(stray_origin);
    composite.add_image(stray);

    // Enough moves to retire a snapshot many times over, and few enough that
    // holding on to every one of them stays cheap
    const int min_moves = 2000;

    // The readers must get through at least one pass while the moves land,
    // or the moves would be testing nothing. How long a pass takes under
    // eight-way contention for the cache depends on the build - it can
    // exceed the minimum under a sanitizer - so keep moving until one does,
    // within reason.
    const int max_moves = 1000 * min_moves;

    std::atomic<bool> stopping {false};
    std::atomic<int> thrown {0};
    std::atomic<int> wrong_values {0};
    std::atomic<int> passes {0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_sampling_threads; t++)
    {
        threads.emplace_back([&] {
            while (!stopping.load())
            {
                sample_seam_row(composite, thrown, wrong_values);
                passes.fetch_add(1);
            }
        });
    }

    // Let every thread get as far as a lookup before moving anything, so that
    // the moves land on a cache that is being read rather than on an idle one
    while (passes.load() < num_sampling_threads)
    {
        std::this_thread::yield();
    }

    const int passes_before_moves = passes.load();

    int num_moves = 0;

    while (num_moves < min_moves ||
           (num_moves < max_moves && passes.load() == passes_before_moves))
    {
        num_moves++;
        stray->move_to(stray_origin + num_moves);
        std::this_thread::yield();
    }

    // If the readers had stopped, the moves above would have landed on nothing
    // and this test would be checking nothing
    EXPECT_GT(passes.load(), passes_before_moves);

    stopping.store(true);

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(thrown.load(), 0);
    EXPECT_EQ(wrong_values.load(), 0);

    // The moved child is still where it was left, and still says nothing about
    // any of the values
    EXPECT_EQ(composite.get_bounds().max_x, stray_origin + num_moves + 1);
    expect_matches_mosaic(
        composite, 0.0, 1.5, 0.25, 0.0, 4 * (SeamMosaic::GRID_SIZE - 1));
}

namespace
{
    // A gridded image, 4x4 unless told otherwise, that counts how often it is
    // sampled, so a test can tell whether a composite skipped it. Band 2 is
    // fully opaque; the data bands hold 10 + x + 4y, which bilinear
    // interpolation reproduces exactly.
    class CountingTile : public rsvp::ImageData
    {
    private:
        mutable int samples = 0;
        int width;
        int height;

    public:
        static constexpr int SIZE = 4;

        explicit CountingTile(int in_width = SIZE, int in_height = SIZE) :
            width(in_width),
            height(in_height)
        {
        }

        int sample_count() const
        {
            return samples;
        }

        static double value_at(double x, double y)
        {
            return 10.0 + x + SIZE * y;
        }

        int get_bands() const override
        {
            return 3;
        }

        int get_width() const override
        {
            return width;
        }

        int get_height() const override
        {
            return height;
        }

        rsvp::TerrainBounds get_bounds() const override
        {
            return pixel_grid_bounds();
        }

        bool
        get_pixel_double(double &value, int x, int y, int band) const override
        {
            samples++;

            if (x < 0 || x >= width || y < 0 || y >= height || band < 0 ||
                band >= 3)
            {
                return false;
            }

            value = (band == 2) ? 255.0 : value_at(x, y);
            return true;
        }
    };

    // A tile that does not say where its pixels are, as an image that has
    // not overridden get_bounds does not. A composite has nothing to skip it
    // by, so it must always be asked.
    class UnboundedTile final : public CountingTile
    {
    public:
        rsvp::TerrainBounds get_bounds() const override
        {
            return rsvp::TerrainBounds();
        }
    };

    template <typename Composite>
    std::shared_ptr<rsvp::CompositeData> make_composite()
    {
        auto composite = std::make_shared<Composite>();
        composite->set_alpha_band(2);
        return composite;
    }

    using CompositeFactory = std::shared_ptr<rsvp::CompositeData> (*)();

    const CompositeFactory composite_factories[] = {
        &make_composite<rsvp::AlphaBlendingCompositeData>,
        &make_composite<rsvp::AverageCompositeData>,
        &make_composite<rsvp::ScoredCompositeData>,
    };
}

// Bounds are where an image's lookups find pixels, and how far past that a
// lookup still lands on one, and both follow the image through whatever
// wraps it.
TEST(composite_data, bounds_follow_the_transform)
{
    // A gridded image on its own is looked up by pixel index, and reaches a
    // pixel past its grid
    const auto tile = std::make_shared<CountingTile>();
    auto bounds = tile->get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 0.0);
    EXPECT_DOUBLE_EQ(bounds.max_x, CountingTile::SIZE - 1);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 1.0);

    // A transform places the grid and stretches the reach with it
    const auto placed =
        std::make_shared<rsvp::TranslatedData>(tile, 10.0, 20.0, 2.0, 0.0);
    bounds = placed->get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, 20.0);
    EXPECT_DOUBLE_EQ(bounds.max_x, 10.0 + 2.0 * (CountingTile::SIZE - 1));
    EXPECT_DOUBLE_EQ(bounds.max_y, 20.0 + 2.0 * (CountingTile::SIZE - 1));
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 2.0);

    // A rotated pixel reaches further along each axis than its pitch
    const rsvp::TranslatedData rotated(tile, 0.0, 0.0, 1.0, M_PI / 4.0);
    EXPECT_DOUBLE_EQ(rotated.get_bounds().pixel_reach, std::sqrt(2.0));

    // Offsetting values moves nothing, so the answer passes through
    const rsvp::ZOffsetData offset(placed);
    bounds = offset.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 2.0);

    // An image that has not said where its pixels are has no bounds, and
    // neither does a transform of it
    const auto unbounded = std::make_shared<UnboundedTile>();
    EXPECT_FALSE(unbounded->get_bounds().valid);
    EXPECT_FALSE(rsvp::TranslatedData(unbounded).get_bounds().valid);

    // A composite's bounds are the union of its children's. Its reach is
    // known only when every child's is: a child that could be anywhere
    // leaves a parent composite nothing safe to skip this one by.
    rsvp::AlphaBlendingCompositeData composite;
    EXPECT_FALSE(composite.get_bounds().valid);

    composite.add_image(placed);
    bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.min_x, 10.0);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 2.0);
    EXPECT_FALSE(bounds.could_reach(10.0 - 2.5, 20.0));
    EXPECT_TRUE(bounds.could_reach(10.0 - 1.5, 20.0));

    composite.add_image(unbounded);
    bounds = composite.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 0.0);
    EXPECT_TRUE(bounds.could_reach(-1000.0, -1000.0));

    composite.remove_image(1);
    EXPECT_DOUBLE_EQ(composite.get_bounds().pixel_reach, 2.0);
}

// A transform over a transform used to place the inner grid as though the
// inner transform were not there
TEST(composite_data, nested_transforms_compose_in_bounds)
{
    const auto tile = std::make_shared<CountingTile>();
    const auto inner =
        std::make_shared<rsvp::TranslatedData>(tile, 10.0, 20.0, 2.0, 0.0);
    const rsvp::TranslatedData outer(inner, 100.0, 200.0, 3.0, 0.0);

    const auto bounds = outer.get_bounds();
    ASSERT_TRUE(bounds.valid);
    EXPECT_DOUBLE_EQ(bounds.pixel_reach, 3.0 * 2.0);
    EXPECT_DOUBLE_EQ(bounds.min_x, 100.0 + 3.0 * 10.0);
    EXPECT_DOUBLE_EQ(bounds.min_y, 200.0 + 3.0 * 20.0);
    EXPECT_DOUBLE_EQ(bounds.max_x,
                     100.0 + 3.0 * (10.0 + 2.0 * (CountingTile::SIZE - 1)));
    EXPECT_DOUBLE_EQ(bounds.max_y,
                     200.0 + 3.0 * (20.0 + 2.0 * (CountingTile::SIZE - 1)));

    // The bounds really do cover the pixels: the corners of the bounds are
    // the corners of the grid
    double value = 0.0;
    EXPECT_TRUE(outer.get_interpolated_pixel_double(
        value, bounds.min_x, bounds.min_y, 0));
    EXPECT_DOUBLE_EQ(value, CountingTile::value_at(0, 0));
    EXPECT_TRUE(outer.get_interpolated_pixel_double(
        value, bounds.max_x, bounds.max_y, 0));
    const double last = CountingTile::SIZE - 1;
    EXPECT_DOUBLE_EQ(value, CountingTile::value_at(last, last));
}

// A composite skips a child that is placed by a transform and is nowhere near
// the point, and never skips one that does not say where its pixels are
TEST(composite_data, a_child_without_bounds_is_never_skipped)
{
    const double far_away = 1000.0;

    for (const CompositeFactory make : composite_factories)
    {
        const auto composite = make();

        const auto placed_tile = std::make_shared<CountingTile>();
        const auto placed = std::make_shared<rsvp::TranslatedData>(
            placed_tile, far_away, far_away, 1.0, 0.0);
        placed->set_alpha_band(2);

        const auto unbounded = std::make_shared<UnboundedTile>();
        unbounded->set_alpha_band(2);

        composite->add_image(placed);
        composite->add_image(unbounded);

        // Inside the unbounded tile's pixels: it answers, and the placed tile
        // must not be consulted
        double value = 0.0;
        EXPECT_TRUE(composite->get_interpolated_pixel_double(
            value, 1.5, 1.5, 0));
        EXPECT_DOUBLE_EQ(value, CountingTile::value_at(1.5, 1.5));
        EXPECT_EQ(placed_tile->sample_count(), 0);
        EXPECT_GT(unbounded->sample_count(), 0);

        // Inside the placed tile: it answers, and the unbounded tile is still
        // asked, since nothing says it is not there too
        const int unbounded_samples = unbounded->sample_count();
        EXPECT_TRUE(composite->get_interpolated_pixel_double(
            value, far_away + 1.5, far_away + 1.5, 0));
        EXPECT_DOUBLE_EQ(value, CountingTile::value_at(1.5, 1.5));
        EXPECT_GT(placed_tile->sample_count(), 0);
        EXPECT_GT(unbounded->sample_count(), unbounded_samples);
    }
}

// Skipping a child by an axis-aligned box around it has to allow for how
// far a pixel of the child reaches along each axis, which under a shear is
// much more than the child's bounds divided by its width would suggest
TEST(composite_data, a_sheared_child_is_not_skipped_where_it_reaches)
{
    // 100 samples by 2 lines, sheared so that each line is offset by 10:
    // x = sample + 10 * line, y = line
    const int width = 100;
    const int height = 2;
    const double shear = 10.0;

    for (const CompositeFactory make : composite_factories)
    {
        const auto composite = make();

        const auto tile = std::make_shared<CountingTile>(width, height);
        const auto sheared = std::make_shared<rsvp::TranslatedData>(
            tile, 0.0, 0.0, 1.0, shear, 0.0, 1.0);
        sheared->set_alpha_band(2);
        composite->add_image(sheared);
        composite->set_interpolating(false);

        // A pixel of the sheared tile reaches `shear` along x for every one
        // it reaches along y
        const auto bounds = sheared->get_bounds();
        ASSERT_TRUE(bounds.valid);
        EXPECT_DOUBLE_EQ(bounds.min_x, 0.0);
        EXPECT_DOUBLE_EQ(bounds.max_x, (width - 1) + shear * (height - 1));
        EXPECT_DOUBLE_EQ(bounds.pixel_reach, 1.0 + shear);

        // (-3, -0.4) inverts to sample 1, line -0.4, which rounds onto the
        // tile's pixel (1, 0), even though it lies 3 units outside the
        // tile's bounds along x
        double value = 0.0;
        EXPECT_TRUE(
            composite->get_interpolated_pixel_double(value, -3.0, -0.4, 0));
        EXPECT_DOUBLE_EQ(value, CountingTile::value_at(1, 0));

        // While a point the tile really cannot reach is still skipped
        const int samples = tile->sample_count();
        EXPECT_FALSE(
            composite->get_interpolated_pixel_double(value, -20.0, -0.4, 0));
        EXPECT_EQ(tile->sample_count(), samples);
    }
}

// A composite remembers which band of each child carries its alpha, and a
// child's alpha band can change after that. Changing it through
// set_alpha_band has to reach every composite the child is in.
TEST(composite_data, scored_composite_follows_a_changed_alpha_band)
{
    // Two three-band children whose bands 1 and 2 disagree about which of
    // them scores higher, so that which band is taken as the score decides
    // which child's band 0 the composite answers with
    const auto plain = std::make_shared<CompositeTestImage>(3);

    const auto rescaled = std::make_shared<rsvp::ZOffsetData>(
        std::make_shared<CompositeTestImage>(3));
    rescaled->set_offset_and_scale(0, 1000.0, 1.0);
    rescaled->set_offset_and_scale(1, 0.0, 100.0);
    rescaled->set_offset_and_scale(2, 0.0, 0.01);

    plain->set_alpha_band(2);
    rescaled->set_alpha_band(2);

    rsvp::ScoredCompositeData composite;
    composite.add_image(plain);
    composite.add_image(rescaled);

    // Scored by band 2, the plain child wins
    double value = 0.0;
    ASSERT_TRUE(composite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));
    EXPECT_DOUBLE_EQ(value, 5.0);

    // Scored by band 1, the rescaled one does
    plain->set_alpha_band(1);
    rescaled->set_alpha_band(1);

    ASSERT_TRUE(composite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));
    EXPECT_DOUBLE_EQ(value, 1005.0);
}

// Skipping a child by its bounds must leave room for the half pixel an
// uninterpolated lookup rounds across
TEST(composite_data, skipping_leaves_room_for_nearest_neighbor_rounding)
{
    for (const CompositeFactory make : composite_factories)
    {
        const auto composite = make();

        const auto tile = std::make_shared<CountingTile>();
        const auto placed =
            std::make_shared<rsvp::TranslatedData>(tile, 0.0, 0.0, 1.0, 0.0);
        placed->set_alpha_band(2);
        composite->add_image(placed);
        composite->set_interpolating(false);

        const double last = CountingTile::SIZE - 1;

        double value = 0.0;
        EXPECT_TRUE(
            composite->get_interpolated_pixel_double(value, -0.4, -0.4, 0));
        EXPECT_DOUBLE_EQ(value, CountingTile::value_at(0, 0));

        EXPECT_TRUE(composite->get_interpolated_pixel_double(
            value, last + 0.4, last + 0.4, 0));
        EXPECT_DOUBLE_EQ(value, CountingTile::value_at(last, last));

        EXPECT_FALSE(
            composite->get_interpolated_pixel_double(value, -0.6, -0.6, 0));
        EXPECT_FALSE(composite->get_interpolated_pixel_double(
            value, last + 0.6, last + 0.6, 0));
    }
}

// The seam and clamped fallbacks skip children by their bounds like the main
// loops do, and so must also decline to skip a child that does not say where
// its pixels are - whether that child is bare or wrapped in an offset
TEST(composite_data, seam_fallbacks_never_skip_a_child_without_bounds)
{
    for (const bool wrap_in_offset : {false, true})
    {
        for (const CompositeFactory make : composite_factories)
        {
            const auto composite = make();

            // Pixels 0..3 in the composite's coordinates, without saying so
            const auto labelled_tile = std::make_shared<UnboundedTile>();
            std::shared_ptr<rsvp::ImageData> labelled = labelled_tile;
            if (wrap_in_offset)
            {
                labelled = std::make_shared<rsvp::ZOffsetData>(labelled_tile);
            }
            labelled->set_alpha_band(2);

            // Pixels 4..7, abutting it with a one-pixel seam between
            const auto placed_tile = std::make_shared<CountingTile>();
            const auto placed = std::make_shared<rsvp::TranslatedData>(
                placed_tile, CountingTile::SIZE, 0.0, 1.0, 0.0);
            placed->set_alpha_band(2);

            composite->add_image(labelled);
            composite->add_image(placed);

            // Neither tile covers the seam on its own
            const double seam_x = CountingTile::SIZE - 0.6;
            const double seam_y = 1.5;

            double labelled_value = 0.0;
            double placed_value = 0.0;
            EXPECT_FALSE(labelled->get_interpolated_pixel_double(
                labelled_value, seam_x, seam_y, 0));
            EXPECT_FALSE(placed->get_interpolated_pixel_double(
                placed_value, seam_x, seam_y, 0));

            // The labelled tile's last column has a weight of 0.6 here and
            // the placed tile's first column 0.4, so together they enclose
            // the point and the composite reconstructs it
            const double labelled_edge =
                CountingTile::value_at(CountingTile::SIZE - 1, seam_y);
            const double placed_edge = CountingTile::value_at(0, seam_y);

            double value = 0.0;
            ASSERT_TRUE(composite->get_interpolated_pixel_double(
                value, seam_x, seam_y, 0));

            if (dynamic_cast<rsvp::ScoredCompositeData *>(composite.get()))
            {
                // A scored composite takes the nearest tile's value
                EXPECT_DOUBLE_EQ(value, labelled_edge);
            }
            else
            {
                EXPECT_DOUBLE_EQ(value,
                                 0.6 * labelled_edge + 0.4 * placed_edge);
            }

            // Off the labelled tile's far edge, only it can supply a clamped
            // sample, and it must not be ruled out
            double weight = 0.0;
            ASSERT_TRUE(composite->get_clamped_pixel_double(
                value, weight, -0.5, seam_y, 0));
            EXPECT_DOUBLE_EQ(weight, 0.5);
            EXPECT_DOUBLE_EQ(value, CountingTile::value_at(0, seam_y));
        }
    }
}

// A child with data at a point but not the alpha band it declares was
// labelled with a band it does not have. A blending composite reports that
// as the mistake it is rather than quietly leaving the pixel out of the
// mosaic; a scored composite, which only ranks children by that band, skips
// the child as it always has. Away from the child it is just no data.
TEST(composite_data, a_missing_alpha_band_is_an_error_where_the_data_is)
{
    for (const CompositeFactory make : composite_factories)
    {
        const auto composite = make();

        const auto image = std::make_shared<CompositeTestImage>(3);
        image->set_alpha_band(7);
        composite->add_image(image);

        double value = 0.0;

        if (dynamic_cast<rsvp::ScoredCompositeData *>(composite.get()))
        {
            EXPECT_NO_THROW(EXPECT_FALSE(
                composite->get_interpolated_pixel_double(value, 1.0, 1.0, 0)));
        }
        else
        {
            EXPECT_THROW(
                composite->get_interpolated_pixel_double(value, 1.0, 1.0, 0),
                std::runtime_error);
        }

        EXPECT_NO_THROW(EXPECT_FALSE(composite->get_interpolated_pixel_double(
            value, -100.0, -100.0, 0)));
    }
}

namespace
{
    // A child whose set_alpha_band keeps the band to itself rather than
    // reaching ImageData::set_alpha_band, so nothing it does invalidates a
    // composite's cached geometry
    class SelfKeepingAlphaImage final : public CompositeTestImage
    {
    private:
        int own_alpha_band = -1;

    public:
        SelfKeepingAlphaImage() :
            CompositeTestImage(3)
        {
        }

        void set_alpha_band(const int band) override
        {
            own_alpha_band = band;
        }

        int get_alpha_band() const override
        {
            return own_alpha_band;
        }
    };
}

// A scored composite scores each child by the band the child says it
// scores by now, not the one it said when the composite last looked
TEST(composite_data, scored_composite_asks_a_child_for_its_alpha_band)
{
    const auto plain = std::make_shared<SelfKeepingAlphaImage>();

    // The rescaled child's band 2 scores between the plain child's band 2
    // and its band 1, so which of those the plain child is scored by decides
    // the winner, and the rescaled child never has to change
    const auto rescaled = std::make_shared<rsvp::ZOffsetData>(
        std::make_shared<CompositeTestImage>(3));
    rescaled->set_offset_and_scale(0, 1000.0, 1.0);
    rescaled->set_offset_and_scale(2, 0.0, 0.5);

    plain->set_alpha_band(2);
    rescaled->set_alpha_band(2);

    rsvp::ScoredCompositeData composite;
    composite.add_image(plain);
    composite.add_image(rescaled);

    // Scored by band 2, the plain child wins
    double value = 0.0;
    ASSERT_TRUE(composite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));
    EXPECT_DOUBLE_EQ(value, 5.0);

    // Scored by band 1, the rescaled one does, even though the plain child
    // told nobody that its band moved
    plain->set_alpha_band(1);

    ASSERT_TRUE(composite.get_interpolated_pixel_double(value, 1.0, 1.0, 0));
    EXPECT_DOUBLE_EQ(value, 1005.0);
}
