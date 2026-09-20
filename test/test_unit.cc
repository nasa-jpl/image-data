#include <csv_data.h>
#include <image_data.h>
#include <mod_data.cc>
#include <mod_data.h>
#include <pgm_data.h>
#include <platform.h>
#include <vicar_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

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

namespace
{
    // Write a one-record-per-line VICAR file: the label, padded to LBLSIZE,
    // followed by the raw pixel bytes as given
    void write_synthetic_vicar(const std::string &path,
                               const std::string &label,
                               const std::vector<uint8_t> &pixels)
    {
        const size_t label_size = 512;
        std::string padded = "LBLSIZE=" + std::to_string(label_size) + "  " +
            label;
        ASSERT_LE(padded.size(), label_size);
        padded.resize(label_size, ' ');

        std::ofstream file(path, std::ofstream::binary | std::ofstream::trunc);
        file << padded;
        file.write(reinterpret_cast<const char *>(pixels.data()),
                   static_cast<std::streamsize>(pixels.size()));
    }
}

// VICAR's BYTE format is unsigned, and the writer already treats it so
TEST(vicar_data, byte_pixels_are_unsigned)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/bytes.vic";

    const std::vector<uint8_t> pixels {0, 1, 127, 128, 200, 255};
    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=6  DIM=3  "
                          "EOL=0  RECSIZE=6  ORG='BSQ'  NL=1  NS=6  NB=1  "
                          "N1=6  N2=1  N3=1  N4=0  NBB=0  NLB=0  "
                          "INTFMT='LOW'  REALFMT='RIEEE'",
                          pixels);

    const auto image = rsvp::VicarData::read_vicarfile(path);
    ASSERT_TRUE(image != nullptr);
    EXPECT_EQ(image->get_pixel_byte_count(), 1);

    for (size_t i = 0; i < pixels.size(); i++)
    {
        double value = -1.0;
        EXPECT_TRUE(image->get_pixel_double(value, static_cast<int>(i), 0, 0));
        EXPECT_EQ(value, pixels[i]);
    }

    // And they survive a round trip through the writer
    const std::string copy_path = tmp_dir + "/bytes_copy.vic";
    image->write_vicarfile(copy_path);
    const auto copy = rsvp::VicarData::read_vicarfile(copy_path);
    ASSERT_TRUE(copy != nullptr);

    for (size_t i = 0; i < pixels.size(); i++)
    {
        double value = -1.0;
        EXPECT_TRUE(copy->get_pixel_double(value, static_cast<int>(i), 0, 0));
        EXPECT_EQ(value, pixels[i]);
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// Integer data stored in the opposite byte order to the host is swapped. The
// files that ship with the tests only cover swapped floating-point data.
TEST(vicar_data, big_endian_half_pixels_are_swapped)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/halves.vic";

    // 1, -2, 300 and -32768 as big-endian 16-bit integers, one band
    // interleaved by pixel so the reshuffle is exercised too: two samples of
    // two bands each
    const std::vector<uint8_t> pixels {
        0x00, 0x01, 0xFF, 0xFE, 0x01, 0x2C, 0x80, 0x00};
    write_synthetic_vicar(path,
                          "FORMAT='HALF'  TYPE='IMAGE'  BUFSIZ=8  DIM=3  "
                          "EOL=0  RECSIZE=8  ORG='BIP'  NL=1  NS=2  NB=2  "
                          "N1=2  N2=2  N3=1  N4=0  NBB=0  NLB=0  "
                          "INTFMT='HIGH'  REALFMT='IEEE'",
                          pixels);

    const auto image = rsvp::VicarData::read_vicarfile(path);
    ASSERT_TRUE(image != nullptr);
    EXPECT_EQ(image->get_pixel_byte_count(), 2);
    EXPECT_EQ(image->get_org(), rsvp::VicarData::BIP);

    double value = 0.0;
    EXPECT_TRUE(image->get_pixel_double(value, 0, 0, 0));
    EXPECT_EQ(value, 1.0);
    EXPECT_TRUE(image->get_pixel_double(value, 0, 0, 1));
    EXPECT_EQ(value, -2.0);
    EXPECT_TRUE(image->get_pixel_double(value, 1, 0, 0));
    EXPECT_EQ(value, 300.0);
    EXPECT_TRUE(image->get_pixel_double(value, 1, 0, 1));
    EXPECT_EQ(value, -32768.0);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A file whose labels continue after the pixels writes back as one whose
// labels do not, and must not claim otherwise
TEST(vicar_data, writing_a_file_with_eol_labels)
{
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    const auto original = rsvp::VicarData::read_vicarfile(
        my_root +
        "/unit_test_data/image_data/"
        "SI0_0040T0670498927_024ECM_T0120004SRLC00700_026100J00.VIC");
    ASSERT_TRUE(original != nullptr);

    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/copy.vic";

    original->write_vicarfile(path);

    std::shared_ptr<rsvp::VicarData> copy;
    ASSERT_NO_THROW(copy = rsvp::VicarData::read_vicarfile(path));
    ASSERT_TRUE(copy != nullptr);

    EXPECT_EQ(copy->get_width(), original->get_width());
    EXPECT_EQ(copy->get_height(), original->get_height());
    EXPECT_EQ(copy->get_bands(), original->get_bands());

    for (int band = 0; band < original->get_bands(); band++)
    {
        for (int y = 0; y < original->get_height(); y += 7)
        {
            for (int x = 0; x < original->get_width(); x += 5)
            {
                double expected = 0.0;
                double actual = 0.0;
                ASSERT_TRUE(original->get_pixel_double(expected, x, y, band));
                ASSERT_TRUE(copy->get_pixel_double(actual, x, y, band));
                EXPECT_EQ(actual, expected);
            }
        }
    }

    // A property that was only in the end-of-line labels comes back too
    std::string value;
    EXPECT_TRUE(copy->get_label_property(
        "MINI_HEADER", "INSTRUMENT_SERIAL_NUMBER", value));
    EXPECT_EQ(value, "164601");

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(image_data, file_extension)
{
    EXPECT_EQ(rsvp::get_file_extension("/a/b/c.IMG"), "img");
    EXPECT_EQ(rsvp::get_file_extension("terrain.mod_tc"), "mod_tc");
    EXPECT_EQ(rsvp::get_file_extension("archive.tar.gz"), "gz");
    EXPECT_EQ(rsvp::get_file_extension("no_extension"), "");
    EXPECT_EQ(rsvp::get_file_extension("/sol.0042/no_extension"), "");
    EXPECT_EQ(rsvp::get_file_extension("trailing."), "");
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
