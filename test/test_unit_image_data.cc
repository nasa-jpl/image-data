#include "csv_data.h"
#include "image_data.h"
#include "pgm_data.h"
#include "translated_data.h"
#include "vicar_data.h"
#include "z_offset_data.h"

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

#define EXPECT_VECTOR_EQ(vector, expected_vector)                             \
    do                                                                        \
    {                                                                         \
        EXPECT_EQ(vector[0], expected_vector[0]);                             \
        EXPECT_EQ(vector[1], expected_vector[1]);                             \
        EXPECT_EQ(vector[2], expected_vector[2]);                             \
    } while (0)

namespace
{
    // The fixture for testing terrains
    class ImageDataTest : public ::testing::Test
    {
    protected:
        ImageDataTest()
        {
            const std::string my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);

            // A non-square 1-byte PGM file with a comment line
            pgm_file =
                my_root + "/unit_test_data/image_data/hemisphere.pgm";

            // A mod file wrapping the above PGM file with a z-scale of
            // 0.5m/px and a z-offset of 10m
            pgm_mod_file =
                my_root + "/unit_test_data/image_data/hemisphere.mod";

            // A 2000x1200 ENav-style CSV file
            csv_file = my_root +
                "/unit_test_data/image_data/flat_cfa-15_slope-mag-25.csv";

            // A heightmap-style little-endian float Vicar file
            ht_file = my_root +
                "/unit_test_data/image_data/"
                "NLB_530659343RASLF0582340NCAM00385M1.ht";

            // An RDR-style big-endian float PDS file
            pds_file = my_root +
                "/unit_test_data/image_data/"
                "NRB_530661097RNG_F0582394NCAM07753M1.IMG";

            // Test the other VICAR pixel organizations
            bil_file = my_root +
                "/unit_test_data/image_data/"
                "vicar_bil.img";
            bip_file = my_root +
                "/unit_test_data/image_data/"
                "vicar_bip.img";

            // This file has EOL properties
            vicar_file = my_root +
                "/unit_test_data/image_data/"
                "SI0_0040T0670498927_024ECM_T0120004SRLC00700_026100J00.VIC";
            // Pick some values at random from the middle
            // Compare these as floats, as the raw data is stored as such
            pds_values[std::make_pair(399, 271)] = 33.19715f;
            pds_values[std::make_pair(512, 605)] = 6.9762964f;
            pds_values[std::make_pair(929, 38)] = 0.0f;
            pds_values[std::make_pair(218, 594)] = 6.5366073f;

            union
            {
                uint16_t two_bytes;
                uint8_t one_byte;
            } num {};
            num.two_bytes = 1;

            host_little_endian = num.one_byte;
        }

        std::string pgm_file;
        std::string pgm_mod_file;
        std::string csv_file;
        std::string mod_file;
        std::string ht_file;
        std::string pds_file;
        std::string bil_file;
        std::string bip_file;
        std::string vicar_file;

        std::map<std::pair<int, int>, float> pds_values;

        bool host_little_endian;
        void
        verify_vicar_values(const std::shared_ptr<rsvp::VicarData> pds) const;
    };

    TEST_F(ImageDataTest, read_pgm)
    {
        auto pgm = rsvp::PGMData::read_pgm(pgm_file);
        ASSERT_TRUE(pgm != nullptr);

        // Image is 542 pixels wide and 494 pixels tall
        EXPECT_EQ(pgm->get_width(), 542);
        EXPECT_EQ(pgm->get_height(), 494);

        // Image has a maximum value of 255, meaning 1 byte/pixel
        EXPECT_EQ(pgm->get_max_value(), 255);
        EXPECT_EQ(pgm->get_pixel_byte_count(), 1);

        // All four corners are white (0xFF = 255);
        int value;
        EXPECT_TRUE(pgm->get_pixel_int(value, 0, 0, 0));
        EXPECT_EQ(value, 255);

        EXPECT_TRUE(pgm->get_pixel_int(value, pgm->get_width() - 1, 0, 0));
        EXPECT_EQ(value, 255);

        EXPECT_TRUE(pgm->get_pixel_int(
            value, pgm->get_width() - 1, pgm->get_height() - 1, 0));
        EXPECT_EQ(value, 255);

        EXPECT_TRUE(pgm->get_pixel_int(value, 0, pgm->get_height() - 1, 0));
        EXPECT_EQ(value, 255);


        // Out-of-bounds data should return "false"
        EXPECT_FALSE(pgm->get_pixel_int(value, -1, 0, 0));
        EXPECT_FALSE(pgm->get_pixel_int(value, 0, -1, 0));
        EXPECT_FALSE(pgm->get_pixel_int(value, 0, pgm->get_height(), 0));
        EXPECT_FALSE(
            pgm->get_pixel_int(value, pgm->get_width(), pgm->get_height(), 0));
        EXPECT_FALSE(pgm->get_pixel_int(value, pgm->get_width(), 0, 0));

        // Poking around in the file, I found these byte values:
        // Byte 67667  -> Value 245
        // Byte 75185  -> Value 86
        // Byte 117390 -> Value 12
        //
        // The data starts at byte 60, and there is one byte per pixel - with
        // 494 lines of 542 pixels, I therefore conclude that those bytes
        // correspond to these lines and samples:
        //
        // Byte 67667  -> Line 124, Sample 399
        // Byte 75185  -> Line 138, Sample 329
        // Byte 117390 -> Line 216, Sample 258

        EXPECT_TRUE(pgm->get_pixel_int(value, 399, 124, 0));
        EXPECT_EQ(value, 245);

        EXPECT_TRUE(pgm->get_pixel_int(value, 329, 138, 0));
        EXPECT_EQ(value, 86);

        EXPECT_TRUE(pgm->get_pixel_int(value, 258, 216, 0));
        EXPECT_EQ(value, 12);

        // Byte 67667 is on an edge - 3 bytes back is 235, and the next byte is
        // 255:
        EXPECT_TRUE(pgm->get_pixel_int(value, 396, 124, 0));
        EXPECT_EQ(value, 235);

        EXPECT_TRUE(pgm->get_pixel_int(value, 397, 124, 0));
        EXPECT_EQ(value, 245);

        EXPECT_TRUE(pgm->get_pixel_int(value, 398, 124, 0));
        EXPECT_EQ(value, 245);

        EXPECT_TRUE(pgm->get_pixel_int(value, 399, 124, 0));
        EXPECT_EQ(value, 245);

        EXPECT_TRUE(pgm->get_pixel_int(value, 400, 124, 0));
        EXPECT_EQ(value, 255);
    }

    TEST_F(ImageDataTest, read_mod_wrapped_pgm)
    {
        auto pgm = rsvp::ImageData::read(pgm_mod_file);

        ASSERT_TRUE(pgm != nullptr);

        double value;

        // These are the same pixel coordinates as in the above test, but the
        // values should be transformed by the mod file's offset of 10m and
        // scale of 0.5m/px
        double offset = 10;
        double scale = 0.5;

        EXPECT_TRUE(pgm->get_pixel_double(value, 396, 124, 0));
        EXPECT_EQ(value, offset + scale * 235);

        EXPECT_TRUE(pgm->get_pixel_double(value, 397, 124, 0));
        EXPECT_EQ(value, offset + scale * 245);

        EXPECT_TRUE(pgm->get_pixel_double(value, 398, 124, 0));
        EXPECT_EQ(value, offset + scale * 245);

        EXPECT_TRUE(pgm->get_pixel_double(value, 399, 124, 0));
        EXPECT_EQ(value, offset + scale * 245);

        EXPECT_TRUE(pgm->get_pixel_double(value, 400, 124, 0));
        EXPECT_EQ(value, offset + scale * 255);
    }

    TEST_F(ImageDataTest, read_csv)
    {
        auto csv = rsvp::CSVData::read_csv(csv_file);

        ASSERT_TRUE(csv != nullptr);

        // File is 1200px wide x 2000px tall
        EXPECT_EQ(csv->get_width(), 1200);
        EXPECT_EQ(csv->get_height(), 2000);

        // Check that we get the corner values right:
        double value;

        EXPECT_TRUE(csv->get_pixel_double(value, 0, 0, 0));
        EXPECT_DOUBLE_EQ(value, 23.2921);

        EXPECT_TRUE(csv->get_pixel_double(value, csv->get_width() - 1, 0, 0));
        EXPECT_DOUBLE_EQ(value, 23.3154);

        EXPECT_TRUE(csv->get_pixel_double(
            value, csv->get_width() - 1, csv->get_height() - 1, 0));
        EXPECT_DOUBLE_EQ(value, -23.2921);

        EXPECT_TRUE(csv->get_pixel_double(value, 0, csv->get_height() - 1, 0));
        EXPECT_DOUBLE_EQ(value, -23.2921);

        // Pick some values at random from the middle
        // Excel cell XC1442 -> -10.2921
        // Excel cell ABM715 ->   6.6182
        // Excel cell RX464  ->  12.5204

        EXPECT_TRUE(csv->get_pixel_double(value, 626, 1441, 0));
        EXPECT_DOUBLE_EQ(value, -10.2921);

        EXPECT_TRUE(csv->get_pixel_double(value, 740, 714, 0));
        EXPECT_DOUBLE_EQ(value, 6.6182);

        EXPECT_TRUE(csv->get_pixel_double(value, 491, 463, 0));
        EXPECT_DOUBLE_EQ(value, 12.5204);
    }

    void ImageDataTest::verify_vicar_values(
        const std::shared_ptr<rsvp::VicarData> ht) const
    {
        // File has 410 wide x 512 tall x 3 band single-precision floats
        EXPECT_EQ(ht->get_width(), 410);
        EXPECT_EQ(ht->get_height(), 512);
        EXPECT_EQ(ht->get_bands(), 3);
        EXPECT_EQ(ht->get_pixel_byte_count(), 4);

        // The vicar file is little-endian - check that either the host is big
        // endian and the VicarData has the opposite_endian flags set, or that
        // the host is little endian and the opposite_endian flags are not set.
        if (!host_little_endian)
        {
            ASSERT_TRUE(ht->is_int_opposite_endian());
            ASSERT_TRUE(ht->is_real_opposite_endian());
        }
        else
        {
            ASSERT_FALSE(ht->is_int_opposite_endian());
            ASSERT_FALSE(ht->is_real_opposite_endian());
        }

        // Pick some values at random from the middle
        // Compare these as floats, as the raw data is stored as such

        double value;

        // A point far away from real data - pure interpolation
        EXPECT_TRUE(ht->get_pixel_double(value, 170, 166, 0));
        EXPECT_DOUBLE_EQ(value, 3.40282346638529e+38);

        EXPECT_TRUE(ht->get_pixel_double(value, 170, 166, 1));
        EXPECT_DOUBLE_EQ(value, -18.590089797973633);

        EXPECT_TRUE(ht->get_pixel_double(value, 170, 166, 2));
        EXPECT_DOUBLE_EQ(value, 1.0f);

        // A defined point surrounded by undefined points - no interpolation
        EXPECT_TRUE(ht->get_pixel_double(value, 297, 336, 0));
        EXPECT_DOUBLE_EQ(value, -18.47114372253418);

        EXPECT_TRUE(ht->get_pixel_double(value, 297, 336, 1));
        EXPECT_DOUBLE_EQ(value, -18.47114372253418);

        EXPECT_TRUE(ht->get_pixel_double(value, 297, 336, 2));
        EXPECT_DOUBLE_EQ(value, 255);

        // An undefined point next to a defined point - partial interpolation
        EXPECT_TRUE(ht->get_pixel_double(value, 139, 266, 0));
        EXPECT_DOUBLE_EQ(value, 3.40282346638529e+38);

        EXPECT_TRUE(ht->get_pixel_double(value, 139, 266, 1));
        EXPECT_DOUBLE_EQ(value, -18.704807281494141);

        EXPECT_TRUE(ht->get_pixel_double(value, 139, 266, 2));
        EXPECT_DOUBLE_EQ(value, 229.5);
    }

    TEST_F(ImageDataTest, read_vicar)
    {
        auto ht = rsvp::VicarData::read_vicarfile(ht_file);

        ASSERT_TRUE(ht != nullptr);
        EXPECT_EQ(ht->get_org(), rsvp::VicarData::DataOrg::BSQ);

        // 410, 4 bytes/px BSQ -> 4096 byte record size
        EXPECT_EQ(ht->get_record_size(), 1640);

        verify_vicar_values(ht);
    }

    TEST_F(ImageDataTest, read_vicar_bil)
    {
        auto ht = rsvp::VicarData::read_vicarfile(bil_file);

        ASSERT_TRUE(ht != nullptr);
        EXPECT_EQ(ht->get_org(), rsvp::VicarData::DataOrg::BIL);

        // Lines are interlaced with bands, records size should
        // remain unchanged compared to BSQ
        EXPECT_EQ(ht->get_record_size(), 1640);

        verify_vicar_values(ht);
    }

    TEST_F(ImageDataTest, read_vicar_bip)
    {
        auto ht = rsvp::VicarData::read_vicarfile(bip_file);

        ASSERT_TRUE(ht != nullptr);
        EXPECT_EQ(ht->get_org(), rsvp::VicarData::DataOrg::BIP);

        // Samples are interlaced with bands, each record will be
        // a collection of every band for every sample,line
        EXPECT_EQ(ht->get_record_size(),
                  ht->get_pixel_byte_count() * ht->get_bands());
        EXPECT_EQ(ht->get_record_size(), 12);

        verify_vicar_values(ht);
    }

    TEST_F(ImageDataTest, read_pds)
    {
        auto pds = rsvp::VicarData::read_vicarfile(pds_file);
        ASSERT_TRUE(pds != nullptr);
        EXPECT_EQ(pds->get_org(), rsvp::VicarData::DataOrg::BSQ);

        // File is 1024px wide x 1024px tall, single-precision float
        EXPECT_EQ(pds->get_width(), 1024);
        EXPECT_EQ(pds->get_height(), 1024);
        EXPECT_EQ(pds->get_bands(), 1);
        EXPECT_EQ(pds->get_pixel_byte_count(), 4);

        // 1024px, 4 bytes/px -> 4096 byte record size
        EXPECT_EQ(pds->get_record_size(), 4096);

        // The PDS file is big-endian - check that either the host is little
        // endian and the VicarData has the opposite_endian flags set, or that
        // the host is big endian and the opposite_endian flags are not set.
        if (host_little_endian)
        {
            ASSERT_TRUE(pds->is_int_opposite_endian());
            ASSERT_TRUE(pds->is_real_opposite_endian());
        }
        else
        {
            ASSERT_FALSE(pds->is_int_opposite_endian());
            ASSERT_FALSE(pds->is_real_opposite_endian());
        }

        // Pick some values at random from the middle
        double image_value;
        for (const auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            double value = pds_value.second;
            // Compare these as floats, as the raw data is stored as such
            EXPECT_TRUE(pds->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(value, image_value);
        }

        // Read out the camera model
        std::string model_type;
        EXPECT_TRUE(pds->get_label_property(
            "GEOMETRIC_CAMERA_MODEL", "MODEL_TYPE", model_type));
        EXPECT_EQ(model_type, "CAHVOR");

        std::string model_frame;
        EXPECT_TRUE(pds->get_label_property("GEOMETRIC_CAMERA_MODEL",
                                            "REFERENCE_COORD_SYSTEM_NAME",
                                            model_frame));
        EXPECT_EQ(model_frame, "ROVER_NAV_FRAME");
        EXPECT_TRUE(pds->get_camera_cahv_frame(model_frame));
        EXPECT_EQ(model_frame, "ROVER_NAV_FRAME");

        double vector[3];
        double expected_c[3] = {1.04154, 0.602222, -1.91595};
        double expected_a[3] = {0.62748, -0.750041, 0.209022};
        double expected_h[3] {1255.92, 400.575, 101.217};
        double expected_v[3] {165.372, -189.475, 1301.23};
        double expected_o[3] {0.626507, -0.749799, 0.212776};
        double expected_r[3] = {1.51942e-05, 0.00176721, -0.00534643};

        EXPECT_TRUE(pds->get_camera_c(vector));
        EXPECT_VECTOR_EQ(vector, expected_c);
        EXPECT_TRUE(pds->get_camera_a(vector));
        EXPECT_VECTOR_EQ(vector, expected_a);
        EXPECT_TRUE(pds->get_camera_h(vector));
        EXPECT_VECTOR_EQ(vector, expected_h);
        EXPECT_TRUE(pds->get_camera_v(vector));
        EXPECT_VECTOR_EQ(vector, expected_v);
        EXPECT_TRUE(pds->get_camera_o(vector));
        EXPECT_VECTOR_EQ(vector, expected_o);
        EXPECT_TRUE(pds->get_camera_r(vector));
        EXPECT_VECTOR_EQ(vector, expected_r);
        EXPECT_FALSE(pds->get_camera_e(vector));

        // Test reading some indexed properties
        std::string array_value;
        EXPECT_TRUE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            2,
                                            array_value));
        EXPECT_EQ(array_value, "0.720058");

        // Make sure that we can get all four fields of the quaternion
        EXPECT_TRUE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            0,
                                            array_value));
        EXPECT_TRUE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            1,
                                            array_value));
        EXPECT_TRUE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            2,
                                            array_value));
        EXPECT_TRUE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            3,
                                            array_value));

        // Make sure we can't get a fifth field!
        EXPECT_FALSE(
            pds->get_label_indexed_property("ARM_COORDINATE_SYSTEM",
                                            "ORIGIN_ROTATION_QUATERNION",
                                            4,
                                            array_value));

        // Make sure we can't get an array value of of a non-array property
        EXPECT_TRUE(pds->get_label_property("GEOMETRIC_CAMERA_MODEL",
                                            "REFERENCE_COORD_SYSTEM_NAME",
                                            array_value));
        EXPECT_FALSE(
            pds->get_label_indexed_property("GEOMETRIC_CAMERA_MODEL",
                                            "REFERENCE_COORD_SYSTEM_NAME",
                                            0,
                                            array_value));
    }

    TEST_F(ImageDataTest, test_transdata)
    {
        auto pds = rsvp::VicarData::read_vicarfile(pds_file);

        ASSERT_TRUE(pds != nullptr);

        auto *trans = new rsvp::TranslatedData(pds);

        double image_value;

        // For the same values in the "read_pds" test above, ensure that the
        // default transformation gives us the same values unchanged
        for (const auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            float value = pds_value.second;

            EXPECT_TRUE(trans->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        // Translate the underlying data by 10 units in X
        trans->set_trans(10, 0, 1, 0, 0, 1);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first + 10;
            int y = pds_value.first.second;
            float value = pds_value.second;

            EXPECT_TRUE(trans->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        // Translate the underlying data by 10 units in X
        trans->set_trans(10, 15, 1, 0, 0, 1);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first + 10;
            int y = pds_value.first.second + 15;
            float value = pds_value.second;

            EXPECT_TRUE(trans->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        // Scale the X-coordinate by a factor of 2
        trans->set_trans(0, 0, 2, 0, 0, 1);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first * 2;
            int y = pds_value.first.second;
            float value = pds_value.second;

            EXPECT_TRUE(trans->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        // Scale and translate
        trans->set_trans(52, -37, 2, 0, 0, 4);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first * 2 + 52;
            int y = pds_value.first.second * 4 - 37;
            float value = pds_value.second;

            EXPECT_TRUE(trans->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        delete (trans);
    }

    TEST_F(ImageDataTest, test_ZOffsetData)
    {
        auto pds = rsvp::VicarData::read_vicarfile(pds_file);

        ASSERT_TRUE(pds != nullptr);

        auto *zoff = new rsvp::ZOffsetData(pds);

        double image_value;

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            float value = pds_value.second;

            EXPECT_TRUE(zoff->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        zoff->set_offset_and_scale(0, 0, 3.0);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            float value = pds_value.second * 3;

            EXPECT_TRUE(zoff->get_pixel_double(image_value, x, y, 0));
            EXPECT_FLOAT_EQ(image_value, value);
        }

        zoff->set_offset_and_scale(0, -37.4, 3.1);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            double value = pds_value.second * 3.1 - 37.4;

            EXPECT_TRUE(zoff->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        // Offsets are applied per band
        zoff->set_offset_and_scale(1, -20.2, 8.5);

        for (auto &pds_value : pds_values)
        {
            int x = pds_value.first.first;
            int y = pds_value.first.second;
            double value = pds_value.second * 3.1 - 37.4;

            EXPECT_TRUE(zoff->get_pixel_double(image_value, x, y, 0));
            EXPECT_DOUBLE_EQ(image_value, value);
        }

        delete (zoff);
    }

    TEST_F(ImageDataTest, test_EOL_properties)
    {
        auto pds = rsvp::VicarData::read_vicarfile(vicar_file);

        ASSERT_TRUE(pds != nullptr);

        // Test reading out some properties that only appear in the EOL label
        std::string value;
        EXPECT_TRUE(pds->get_label_property(
            "IDENTIFICATION", "ACTIVE_FLIGHT_STRING_ID", value));
        EXPECT_EQ(value, "A");

        EXPECT_TRUE(pds->get_label_property(
            "MINI_HEADER", "INSTRUMENT_SERIAL_NUMBER", value));
        EXPECT_EQ(value, "164601");
    }
}
