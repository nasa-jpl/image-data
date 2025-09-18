#include <csv_data.h>
#include <image_data.h>
#include <mod_data.cc>
#include <mod_data.h>
#include <pgm_data.h>
#include <platform.h>
#include <vicar_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

TEST(mod_data, apply_properties_exceptions_num_tokens)
{
    // Path to test image file
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    std::string pgm_mod_file =
        my_root + "/unit_test_data/image_data/hemisphere.mod";
    auto img_ptr = rsvp::ImageData::read(pgm_mod_file);
    std::list<std::string> tokens = {"[", "]"};
    EXPECT_THROW(rsvp::apply_properties(img_ptr, &tokens, ""),
                 std::runtime_error);
}

TEST(mod_data, apply_properties_exceptions_bad_delimiter)
{
    // Path to test image file
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    std::string pgm_mod_file =
        my_root + "/unit_test_data/image_data/hemisphere.mod";
    auto img_ptr = rsvp::ImageData::read(pgm_mod_file);
    std::list<std::string> tokens = {"{", "alpha_band", "100", "}"};
    EXPECT_THROW(rsvp::apply_properties(img_ptr, &tokens, ""),
                 std::runtime_error);
}

TEST(mod_data, apply_properties_exceptions_invalid_prop)
{
    // Path to test image file
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    std::string pgm_mod_file =
        my_root + "/unit_test_data/image_data/hemisphere.mod";
    auto img_ptr = rsvp::ImageData::read(pgm_mod_file);
    std::list<std::string> tokens = {"[", "not_a_prop", "]"};
    EXPECT_THROW(rsvp::apply_properties(img_ptr, &tokens, ""),
                 std::runtime_error);
}

TEST(mod_data, apply_properties_exceptions_bad_alpha)
{
    // Path to test image file
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    std::string pgm_mod_file =
        my_root + "/unit_test_data/image_data/hemisphere.mod";
    auto img_ptr = rsvp::ImageData::read(pgm_mod_file);
    std::list<std::string> tokens = {"[", "alpha_band", "]"};
    EXPECT_THROW(rsvp::apply_properties(img_ptr, &tokens, ""),
                 std::runtime_error);
    tokens = {"[", "alpha_band", "notanumber", "]"};
    EXPECT_THROW(rsvp::apply_properties(img_ptr, &tokens, ""),
                 std::runtime_error);
}

TEST(mod_data, parse_imagedata_exceptions_invalid_args)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test.mod";
    std::ofstream ofs;

    // bad delimiters
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "[ composite 1 2 ]" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // missing ending }
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ composite 1 2 " << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // missing starting {
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "composite 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // invalid block argument name
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ not-an-option 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(mod_data, parse_zoffset_exceptions)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test_zoffset.mod";
    std::ofstream ofs;

    // Not enough arguments
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // Wrong block delimiters
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset [] 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // first arg block missing ending }
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset { 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // zoffset missing offset or scale value
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset { } 1 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // zoffset invalid values for offset
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset { } not-a-double 1 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // zoffset invalid values for scale
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ zoffset { } 1 not-a-double }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(mod_data, parse_deinterpolate_exceptions)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test_deinterpolate.mod";
    std::ofstream ofs;

    // not enough arguments
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ deinterpolate }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(mod_data, parse_filedata_exceptions)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test_filedata.mod";
    std::ofstream ofs;

    // not enough arguments
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ file }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // not an absolute file path
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ file not/a/absolute/path }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // invalid extension
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ file /file.wrong }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(mod_data, parse_compdata_exceptions)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test_compdata.mod";
    std::ofstream ofs;

    // not enough arguments
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ composite { } }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // wrong delimiters
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ composite [ ] }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // block not closed
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ composite { 1 2 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(mod_data, parse_transdata_exceptions)
{
    // create a test .mod file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test_transdata.mod";
    std::ofstream ofs;

    // not enough arguments
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl << "{ transform }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    // parameter not a double
    ofs.open(filename, std::ofstream::trunc);
    ofs << "I~" << std::endl
        << "{ transform { } not-a-double 2 3 4 5 6 }" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(csv_data, read_csv_exceptions)
{
    // invalid file
    EXPECT_THROW(rsvp::CSVData::read_csv("invalid-file-name"),
                 std::runtime_error);

    // create a test .csv file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test.csv";
    std::ofstream ofs;

    // csv is empty
    ofs.open(filename, std::ofstream::trunc);
    ofs.close();
    EXPECT_THROW(rsvp::CSVData::read_csv(filename), std::runtime_error);

    // non-rectangular csv
    ofs.open(filename, std::ofstream::trunc);
    ofs << "1,1,1," << std::endl << "1,1,";
    ofs.close();
    EXPECT_THROW(rsvp::CSVData::read_csv(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(image_data, read_exceptions)
{
    EXPECT_THROW(rsvp::ImageData::read("invalid.extension"),
                 std::runtime_error);
    EXPECT_THROW(rsvp::ImageData::read("file-doesnt-exist"),
                 std::runtime_error);
}

TEST(pgm_data, read_pgm_exceptions)
{
    // invalid file
    EXPECT_THROW(rsvp::PGMData::read_pgm("invalid-file-name"),
                 std::runtime_error);

    // create a test .pgm file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test.pgm";
    std::ofstream ofs;

    // first token is NOT P5
    ofs.open(filename, std::ofstream::trunc);
    ofs << "notp5" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::PGMData::read_pgm(filename), std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(vicar_data, file_writing)
{
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    std::string ht_file = my_root +
        "/unit_test_data/image_data/NLB_530659343RASLF0582340NCAM00385M1.ht";
    auto vicar_data = rsvp::VicarData::read_vicarfile(ht_file);

    // create a test vicardata file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    vicar_data->write_vicarfile(tmp_dir +
                                "/NLB_530659343RASLF0582340NCAM00385M1.ht");

    auto vicar_new = rsvp::VicarData::read_vicarfile(
        tmp_dir + "/NLB_530659343RASLF0582340NCAM00385M1.ht");

    double first;
    vicar_data->get_pixel_double(first, 100, 100, 1);
    double second;
    vicar_new->get_pixel_double(second, 100, 100, 1);
    EXPECT_EQ(first, second);
}

TEST(vicar_data, file_synthesis)
{
    // create a test vicardata file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);

    rsvp::VicarData vicar_data(2, 2, 1, rsvp::VicarData::REAL);
    vicar_data.set_pixel_double(1.0, 0, 0, 0);
    vicar_data.set_pixel_double(2.0, 1, 0, 0);
    vicar_data.set_pixel_double(3.0, 0, 1, 0);
    vicar_data.set_pixel_double(4.0, 1, 1, 0);

    vicar_data.write_vicarfile(tmp_dir + "/synth.ht");

    auto vicar_new = rsvp::VicarData::read_vicarfile(tmp_dir + "/synth.ht");

    double first;
    vicar_data.get_pixel_double(first, 1, 1, 0);
    double second;
    vicar_new->get_pixel_double(second, 1, 1, 0);
    EXPECT_EQ(first, 4.0);
    EXPECT_NEAR(first, second, 0.0001);
}

TEST(vicar_data, invalid_file)
{
    // file does not exist
    EXPECT_THROW(rsvp::VicarData::read_vicarfile("invalid_filename"),
                 std::runtime_error);

    // create a test vicardata file
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test.vicar";
    std::ofstream ofs;

    // file missing RECORD_BYTES or LABEL_RECORDS
    ofs.open(filename, std::ofstream::trunc);
    ofs << "ODL_VERSION_ID" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::VicarData::read_vicarfile(filename),
                 std::runtime_error);

    // first token not LDLSIZE
    ofs.open(filename, std::ofstream::trunc);
    ofs << "missing proper first token" << std::endl;
    ofs.close();
    EXPECT_THROW(rsvp::VicarData::read_vicarfile(filename),
                 std::runtime_error);
    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(platform, endian)
{
    static const struct EndianTestVector
    {
        union
        {
            uint8_t bytes[8];
            uint64_t u64;
        } orders[2];
        int size;
    } test_values_const[] = {
        // The 10X values should not be touched so they are
        // in the same byte order in both arrays
        {{{{1, 102, 103, 104, 105, 106, 107, 108}},
          {{1, 102, 103, 104, 105, 106, 107, 108}}},
         1},

        {{{{1, 2, 103, 104, 105, 106, 107, 108}},
          {{2, 1, 103, 104, 105, 106, 107, 108}}},
         2},

        {{{{1, 2, 3, 4, 105, 106, 107, 108}},
          {{4, 3, 2, 1, 105, 106, 107, 108}}},
         4},

        {{{{1, 2, 3, 4, 5, 6, 7, 8}}, {{8, 7, 6, 5, 4, 3, 2, 1}}}, 8},
    };

    struct EndianTestVector test_values_in_place[4];
    memcpy(test_values_in_place, test_values_const, sizeof(test_values_const));

    // Test in-place byte swapping
    for (auto &i : test_values_in_place)
    {
        // Perform the byte swap on the first value in the array and
        // test the value against the second value
        rsvp::byte_swap_inplace(i.orders[0].bytes, i.size);
        EXPECT_EQ(i.orders[0].u64, i.orders[1].u64) << "Size: " << i.size;
    }

    // Test out-of-place byte swapping
    for (const auto &i : test_values_const)
    {
        union
        {
            uint8_t bytes[8];
            uint64_t u64;
        } result = {{101, 102, 103, 104, 105, 106, 107, 108}};
        rsvp::byte_swap(reinterpret_cast<unsigned char *>(&result),
                        i.orders[0].bytes,
                        i.size);
        EXPECT_EQ(result.u64, i.orders[1].u64) << "Size: " << i.size;
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
