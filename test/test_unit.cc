#include <composite_data.h>
#include <csv_data.h>
#include <image_data.h>
#include <mod_data.cc>
#include <mod_data.h>
#include <pgm_data.h>
#include <platform.h>
#include <translated_data.h>
#include <vicar_data.h>
#include <z_offset_data.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include <clocale>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <list>
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

// A host that has switched the process to a comma-decimal locale must not
// change what a CSV reads as
TEST(csv_data, fields_read_the_same_in_any_locale)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string filename = tmp_dir + "/locale.csv";

    {
        std::ofstream ofs(filename, std::ofstream::trunc);
        ofs << "2.5,1,7\n"
            << "1.5,-2.25,3e2\n";
    }

    const auto check = [&filename] {
        const auto csv = rsvp::CSVData::read_csv(filename);
        ASSERT_TRUE(csv != nullptr);
        EXPECT_EQ(csv->get_width(), 3);
        EXPECT_EQ(csv->get_height(), 2);

        const double expected[2][3] = {{2.5, 1.0, 7.0}, {1.5, -2.25, 300.0}};

        for (int y = 0; y < 2; y++)
        {
            for (int x = 0; x < 3; x++)
            {
                double value = 0.0;
                EXPECT_TRUE(csv->get_pixel_double(value, x, y, 0));
                EXPECT_DOUBLE_EQ(value, expected[y][x])
                    << "at (" << x << ", " << y << ")";
            }
        }
    };

    check();

    // Not every machine has a comma-decimal locale installed; where one is,
    // reading under it must agree with reading under the C locale
    const char *const comma_locales[] = {
        "de_DE.UTF-8", "de_DE", "fr_FR.UTF-8"};

    for (const char *const name : comma_locales)
    {
        if (std::setlocale(LC_NUMERIC, name) == nullptr)
        {
            continue;
        }

        check();
        std::setlocale(LC_NUMERIC, "C");
        break;
    }

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

TEST(vicar_data, binary_prefix_bytes)
{
    // None of the checked in files sets NBB, so build one that does. The
    // image area is N2*N3 records, each NBB prefix bytes then N1 pixels.
    const int n1 = 2, n2 = 3, n3 = 5, nbb = 4;
    const int record_size = nbb + n1;
    const int lblsize = 512;

    std::stringstream labels;
    labels << "LBLSIZE=" << lblsize
           << " FORMAT='BYTE' TYPE='IMAGE' BUFSIZ=" << record_size
           << " DIM=3 EOL=0 RECSIZE=" << record_size << " ORG='BSQ' NL=" << n2
           << " NS=" << n1 << " NB=" << n3 << " N1=" << n1 << " N2=" << n2
           << " N3=" << n3 << " N4=0 NBB=" << nbb
           << " NLB=0 HOST='X64-64-LINX' INTFMT='LOW' REALFMT='RIEEE' "
              "BHOST='X64-64-LINX' BINTFMT='LOW' BREALFMT='RIEEE' BLTYPE='' ";
    std::string header = labels.str();
    header.resize(static_cast<size_t>(lblsize), ' ');

    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/nbb.ht";

    std::ofstream ofs(path, std::ios::binary);
    ofs << header;
    for (int record = 0; record < n2 * n3; record++)
    {
        for (int i = 0; i < nbb; i++)
        {
            ofs.put(static_cast<char>(0xEE)); // prefix, not pixel data
        }
        for (int sample = 0; sample < n1; sample++)
        {
            ofs.put(static_cast<char>(record * n1 + sample));
        }
    }
    ofs.close();

    auto vicar_data = rsvp::VicarData::read_vicarfile(path);

    // BSQ, so a record is one line of one band.
    for (int band = 0; band < n3; band++)
    {
        for (int line = 0; line < n2; line++)
        {
            for (int sample = 0; sample < n1; sample++)
            {
                double value = -1.0;
                ASSERT_TRUE(
                    vicar_data->get_pixel_double(value, sample, line, band));
                EXPECT_EQ(
                    static_cast<double>((band * n2 + line) * n1 + sample),
                    value);
            }
        }
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
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

// HALF and FULL are signed, and the writer must keep negative values so
TEST(vicar_data, negative_integer_pixels_round_trip)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);

    const struct
    {
        rsvp::VicarData::DataFormat format;
        double values[2];
    } cases[] = {
        {rsvp::VicarData::HALF, {-5.0, 32767.0}},
        {rsvp::VicarData::FULL, {-70000.0, 70000.0}},
    };

    for (const auto &c : cases)
    {
        rsvp::VicarData image(2, 1, 1, c.format);
        image.set_pixel_double(c.values[0], 0, 0, 0);
        image.set_pixel_double(c.values[1], 1, 0, 0);

        const std::string path = tmp_dir + "/signed.vic";
        image.write_vicarfile(path);

        const auto copy = rsvp::VicarData::read_vicarfile(path);
        ASSERT_TRUE(copy != nullptr);

        for (int x = 0; x < 2; x++)
        {
            double value = 0.0;
            EXPECT_TRUE(copy->get_pixel_double(value, x, 0, 0));
            EXPECT_EQ(value, c.values[x]);
        }
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A quote inside a label value is escaped by doubling it, on the way in and
// on the way back out
TEST(vicar_data, quotes_in_labels_round_trip)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/quoted.vic";

    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=1  DIM=3  "
                          "EOL=0  RECSIZE=1  ORG='BSQ'  NL=1  NS=1  NB=1  "
                          "N1=1  N2=1  N3=1  N4=0  NBB=0  NLB=0  "
                          "INTFMT='LOW'  REALFMT='RIEEE'  "
                          "PROPERTY='NOTES'  OWNER='O''Brien'",
                          {42});

    const auto image = rsvp::VicarData::read_vicarfile(path);
    ASSERT_TRUE(image != nullptr);

    std::string owner;
    EXPECT_TRUE(image->get_label_property("NOTES", "OWNER", owner));
    EXPECT_EQ(owner, "O'Brien");

    const std::string copy_path = tmp_dir + "/quoted_copy.vic";
    image->write_vicarfile(copy_path);

    const auto copy = rsvp::VicarData::read_vicarfile(copy_path);
    ASSERT_TRUE(copy != nullptr);

    owner.clear();
    EXPECT_TRUE(copy->get_label_property("NOTES", "OWNER", owner));
    EXPECT_EQ(owner, "O'Brien");

    double value = 0.0;
    EXPECT_TRUE(copy->get_pixel_double(value, 0, 0, 0));
    EXPECT_EQ(value, 42.0);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A quoted string that happens to start with '(' is still a string, and has
// to be written back as one: written bare, the reader would take it for a
// list and lose every label after it
TEST(vicar_data, strings_starting_with_a_paren_round_trip)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/paren.vic";

    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=1  DIM=3  "
                          "EOL=0  RECSIZE=1  ORG='BSQ'  NL=1  NS=1  NB=1  "
                          "N1=1  N2=1  N3=1  N4=0  NBB=0  NLB=0  "
                          "INTFMT='LOW'  REALFMT='RIEEE'  "
                          "PROPERTY='NOTES'  REMARK='(see fig. 1) revised'  "
                          "SCALE=(2.0, 4.0)  OWNER='O''Brien'",
                          {42});

    const auto image = rsvp::VicarData::read_vicarfile(path);
    ASSERT_TRUE(image != nullptr);

    const std::string copy_path = tmp_dir + "/paren_copy.vic";
    image->write_vicarfile(copy_path);

    const auto copy = rsvp::VicarData::read_vicarfile(copy_path);
    ASSERT_TRUE(copy != nullptr);

    for (const auto &read : {image, copy})
    {
        std::string value;
        EXPECT_TRUE(read->get_label_property("NOTES", "REMARK", value));
        EXPECT_EQ(value, "(see fig. 1) revised");

        // A real list is still written bare, and read back as one
        value.clear();
        EXPECT_TRUE(read->get_label_property("NOTES", "SCALE", value));
        EXPECT_EQ(value, "(2.0, 4.0)");

        // And the label after the troublesome one survives
        value.clear();
        EXPECT_TRUE(read->get_label_property("NOTES", "OWNER", value));
        EXPECT_EQ(value, "O'Brien");
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// The labels the writer works out for itself must not be repeated from the
// labels that were read in, whatever they said
TEST(vicar_data, system_labels_are_written_once)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/system.vic";

    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=1  DIM=3  "
                          "EOL=0  RECSIZE=1  ORG='BSQ'  NL=1  NS=1  NB=1  "
                          "N1=1  N2=1  N3=1  N4=7  NBB=0  NLB=0  "
                          "HOST='SOMEHOST'  INTFMT='LOW'  REALFMT='RIEEE'  "
                          "BHOST='OTHERHOST'  BLTYPE='SOMETHING'  "
                          "MISSION='M2020'",
                          {42});

    const auto image = rsvp::VicarData::read_vicarfile(path);
    ASSERT_TRUE(image != nullptr);

    const std::string copy_path = tmp_dir + "/system_copy.vic";
    image->write_vicarfile(copy_path);

    std::string label;
    {
        std::ifstream copy_file(copy_path, std::ifstream::binary);
        std::getline(copy_file, label, '\0');
    }

    const auto occurrences = [&label](const std::string &text) {
        int count = 0;
        for (size_t at = label.find(text); at != std::string::npos;
             at = label.find(text, at + 1))
        {
            count++;
        }
        return count;
    };

    EXPECT_EQ(occurrences(" HOST="), 1);
    EXPECT_EQ(occurrences(" BHOST="), 1);
    EXPECT_EQ(occurrences(" BLTYPE="), 1);
    EXPECT_EQ(occurrences(" N4="), 1);
    EXPECT_EQ(occurrences(" EOL="), 1);
    EXPECT_EQ(occurrences("SOMEHOST"), 0);
    EXPECT_EQ(occurrences("N4=7"), 0);

    // While a label the writer does not work out for itself is kept
    EXPECT_EQ(occurrences(" MISSION='M2020'"), 1);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A value that does not fit the file's integer format saturates rather than
// being converted out of range, which is undefined and comes out differently
// on different processors
TEST(vicar_data, out_of_range_values_saturate_when_written)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);

    {
        rsvp::VicarData image(4, 1, 1, rsvp::VicarData::BYTE);
        image.set_pixel_double(300.0, 0, 0, 0);
        image.set_pixel_double(-1.0, 1, 0, 0);
        image.set_pixel_double(42.7, 2, 0, 0);
        image.set_pixel_double(std::nan(""), 3, 0, 0);

        const std::string path = tmp_dir + "/bytes.vic";
        image.write_vicarfile(path);

        const auto copy = rsvp::VicarData::read_vicarfile(path);
        ASSERT_TRUE(copy != nullptr);

        const double expected[] = {255.0, 0.0, 42.0, 0.0};
        for (int x = 0; x < 4; x++)
        {
            double value = -1.0;
            EXPECT_TRUE(copy->get_pixel_double(value, x, 0, 0));
            EXPECT_EQ(value, expected[x]) << "at " << x;
        }
    }

    {
        rsvp::VicarData image(2, 1, 1, rsvp::VicarData::HALF);
        image.set_pixel_double(40000.0, 0, 0, 0);
        image.set_pixel_double(-40000.0, 1, 0, 0);

        const std::string path = tmp_dir + "/halves.vic";
        image.write_vicarfile(path);

        const auto copy = rsvp::VicarData::read_vicarfile(path);
        ASSERT_TRUE(copy != nullptr);

        double value = 0.0;
        EXPECT_TRUE(copy->get_pixel_double(value, 0, 0, 0));
        EXPECT_EQ(value, 32767.0);
        EXPECT_TRUE(copy->get_pixel_double(value, 1, 0, 0));
        EXPECT_EQ(value, -32768.0);
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// Interpolating straight out of the pixel buffer must agree with the default
// implementation everywhere, edges and all
TEST(vicar_data, interpolates_like_the_default)
{
    rsvp::VicarData image(3, 3, 1, rsvp::VicarData::REAL);
    for (int y = 0; y < 3; y++)
    {
        for (int x = 0; x < 3; x++)
        {
            image.set_pixel_double(1.0 + x + 3 * y, x, y, 0);
        }
    }

    double value = 0.0;

    // Interior, last row and column, and just past them
    EXPECT_TRUE(image.get_interpolated_pixel_double(value, 0.5, 0.5, 0));
    EXPECT_DOUBLE_EQ(value, 3.0);
    EXPECT_TRUE(image.get_interpolated_pixel_double(value, 2.0, 2.0, 0));
    EXPECT_DOUBLE_EQ(value, 9.0);
    EXPECT_TRUE(image.get_interpolated_pixel_double(value, 2.0, 0.5, 0));
    EXPECT_DOUBLE_EQ(value, 4.5);
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 2.25, 2.0, 0));
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 2.0, 2.25, 0));

    // Between -1 and 0 is outside, not a reflection about the edge
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, -0.25, 0.0, 0));
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 0.0, -0.25, 0));

    // Bands are checked too
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 1.0, 1.0, 1));
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 1.0, 1.0, -1));

    // Every answer matches the one the default arrives at through
    // get_pixel_double
    for (int interpolating = 0; interpolating < 2; interpolating++)
    {
        image.set_interpolating(interpolating);

        for (double y = -1.5; y <= 3.5; y += 0.125)
        {
            for (double x = -1.5; x <= 3.5; x += 0.125)
            {
                double fast = 0.0;
                double slow = 0.0;
                const bool fast_ok =
                    image.get_interpolated_pixel_double(fast, x, y, 0);
                const bool slow_ok =
                    image.ImageData::get_interpolated_pixel_double(
                        slow, x, y, 0);
                ASSERT_EQ(fast_ok, slow_ok) << x << ", " << y;
                if (fast_ok)
                {
                    ASSERT_EQ(fast, slow) << x << ", " << y;
                }
            }
        }
    }

    // Nearest neighbor reaches half a pixel past the edges
    image.set_interpolating(false);
    EXPECT_TRUE(image.get_interpolated_pixel_double(value, -0.4, -0.4, 0));
    EXPECT_DOUBLE_EQ(value, 1.0);
    EXPECT_TRUE(image.get_interpolated_pixel_double(value, 2.4, 2.4, 0));
    EXPECT_DOUBLE_EQ(value, 9.0);
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, -0.6, 0.0, 0));
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 2.6, 0.0, 0));
}

// Sixteen-bit PGM pixels are stored most significant byte first
TEST(pgm_data, sixteen_bit_pixels_are_big_endian)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/wide.pgm";

    {
        std::ofstream file(path, std::ofstream::binary | std::ofstream::trunc);
        file << "P5\n# a comment\n2 1\n65535\n";
        const uint8_t pixels[] = {0x01, 0x02, 0xFF, 0xFE};
        file.write(reinterpret_cast<const char *>(pixels), sizeof(pixels));
    }

    const auto image = rsvp::PGMData::read_pgm(path);
    ASSERT_TRUE(image != nullptr);
    EXPECT_EQ(image->get_width(), 2);
    EXPECT_EQ(image->get_height(), 1);

    int value = 0;
    EXPECT_TRUE(image->get_pixel_int(value, 0, 0, 0));
    EXPECT_EQ(value, 0x0102);
    EXPECT_TRUE(image->get_pixel_int(value, 1, 0, 0));
    EXPECT_EQ(value, 0xFFFE);

    double real_value = 0.0;
    EXPECT_TRUE(image->get_pixel_double(real_value, 1, 0, 0));
    EXPECT_EQ(real_value, 65534.0);
    EXPECT_FALSE(image->get_pixel_double(real_value, 2, 0, 0));

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// Fields are whatever lies between commas: empty or unparseable ones read as
// zero, and a trailing comma adds nothing
TEST(csv_data, fields_parse_as_numbers)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/fields.csv";

    {
        std::ofstream file(path, std::ofstream::trunc);
        file << "1,2.5,-3e2,\n"
             << ",  4,x\n";
    }

    const auto image = rsvp::CSVData::read_csv(path);
    ASSERT_TRUE(image != nullptr);
    EXPECT_EQ(image->get_width(), 3);
    EXPECT_EQ(image->get_height(), 2);

    const double expected[2][3] = {{1.0, 2.5, -300.0}, {0.0, 4.0, 0.0}};
    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 3; x++)
        {
            double value = -1.0;
            EXPECT_TRUE(image->get_pixel_double(value, x, y, 0));
            EXPECT_EQ(value, expected[y][x]) << x << ", " << y;
        }
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// Every record of a VICAR file - not every N1 - carries its own binary
// prefix, and the pixels come after it
TEST(vicar_data, binary_prefixes_precede_every_record)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/prefixed.vic";

    // Three lines of two samples, band interleaved by line, each record led
    // by a two-byte prefix that is not pixel data. More records than there
    // are samples in one, so that counting a prefix per N1 would come up
    // short.
    const std::vector<uint8_t> records {
        0xAA, 0xBB, 1, 2, //
        0xAA, 0xBB, 3, 4, //
        0xAA, 0xBB, 5, 6, //
    };
    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=4  DIM=3  "
                          "EOL=0  RECSIZE=4  ORG='BIL'  NL=3  NS=2  NB=1  "
                          "N1=2  N2=1  N3=3  N4=0  NBB=2  NLB=0  "
                          "INTFMT='LOW'  REALFMT='RIEEE'",
                          records);

    std::shared_ptr<rsvp::VicarData> image;
    ASSERT_NO_THROW(image = rsvp::VicarData::read_vicarfile(path));
    ASSERT_TRUE(image != nullptr);
    EXPECT_EQ(image->get_binary_prefix_byte_count(), 2);

    for (int y = 0; y < 3; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            double value = -1.0;
            EXPECT_TRUE(image->get_pixel_double(value, x, y, 0));
            EXPECT_EQ(value, 1 + x + 2 * y) << x << ", " << y;
        }
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A file that ends before its pixels do is refused, not padded with zeros
TEST(vicar_data, a_truncated_file_is_an_error)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/short.vic";

    // Two lines of three samples promised, one and a half delivered
    const std::vector<uint8_t> pixels {1, 2, 3, 4};
    write_synthetic_vicar(path,
                          "FORMAT='BYTE'  TYPE='IMAGE'  BUFSIZ=3  DIM=3  "
                          "EOL=0  RECSIZE=3  ORG='BSQ'  NL=2  NS=3  NB=1  "
                          "N1=3  N2=2  N3=1  N4=0  NBB=0  NLB=0  "
                          "INTFMT='LOW'  REALFMT='RIEEE'",
                          pixels);

    EXPECT_THROW(rsvp::VicarData::read_vicarfile(path), std::runtime_error);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A PGM that ends before its pixels do is refused too
TEST(pgm_data, a_truncated_file_is_an_error)
{
    const std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    const std::string path = tmp_dir + "/short.pgm";

    std::ofstream file(path, std::ofstream::binary | std::ofstream::trunc);
    file << "P5\n2 2\n255\n";
    file.put(1);
    file.put(2);
    file.put(3);
    file.close();

    EXPECT_THROW(rsvp::PGMData::read_pgm(path), std::runtime_error);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

// A coordinate too far out to hold in an int is outside every image, and
// must say so rather than be cast
TEST(vicar_data, coordinates_beyond_int_range_are_out_of_bounds)
{
    const auto shared =
        std::make_shared<rsvp::VicarData>(3, 3, 1, rsvp::VicarData::REAL);
    rsvp::VicarData &image = *shared;
    for (int y = 0; y < 3; y++)
    {
        for (int x = 0; x < 3; x++)
        {
            image.set_pixel_double(1.0, x, y, 0);
        }
    }

    const double far_out[] = {3.0e9,
                              -3.0e9,
                              1.0e300,
                              std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()};

    for (int interpolating = 0; interpolating < 2; interpolating++)
    {
        image.set_interpolating(interpolating);

        for (const double coordinate : far_out)
        {
            double value = 0.0;
            EXPECT_FALSE(
                image.get_interpolated_pixel_double(value, coordinate, 1.0, 0))
                << coordinate;
            EXPECT_FALSE(
                image.get_interpolated_pixel_double(value, 1.0, coordinate, 0))
                << coordinate;
            EXPECT_FALSE(image.ImageData::get_interpolated_pixel_double(
                value, coordinate, 1.0, 0))
                << coordinate;

            // And through a transform whose inverse lands there
            const rsvp::TranslatedData placed(
                shared, coordinate, 0.0, 1.0, 0.0);
            EXPECT_FALSE(placed.get_interpolated_pixel_double(
                value, 1.0 - coordinate, 1.0, 0))
                << coordinate;
        }
    }

    // Right at the edge of what fits, the answer is still just "outside"
    double value = 0.0;
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, 2147483646.5, 1.0, 0));
    EXPECT_FALSE(image.get_interpolated_pixel_double(value, -2147483647.5, 1.0, 0));
}

// An alpha_band the image does not have is a mistake in the .mod file, and
// is reported as one when the file is read rather than when the image is
// first blended
TEST(mod_data, alpha_band_out_of_range_is_an_error)
{
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    const auto image = rsvp::VicarData::read_vicarfile(
        my_root +
        "/unit_test_data/image_data/NLB_530659343RASLF0582340NCAM00385M1.ht");
    ASSERT_TRUE(image != nullptr);
    ASSERT_EQ(image->get_bands(), 3);

    std::list<std::string> too_high = {"[", "alpha_band", "3", "]"};
    EXPECT_THROW(rsvp::apply_properties(image, &too_high, "test.mod"),
                 std::runtime_error);

    std::list<std::string> in_range = {"[", "alpha_band", "2", "]"};
    EXPECT_TRUE(rsvp::apply_properties(image, &in_range, "test.mod"));
    EXPECT_EQ(image->get_alpha_band(), 2);

    // Negative means no alpha band, which any image can have
    std::list<std::string> none = {"[", "alpha_band", "-1", "]"};
    EXPECT_TRUE(rsvp::apply_properties(image, &none, "test.mod"));
    EXPECT_EQ(image->get_alpha_band(), -1);
}

// A single-band image is blended as opaque and its alpha band never read, so
// any alpha_band is allowed on it, as it is on a composite that reports the
// single band of its first child: the .mod defaults label such images with
// band 2 unasked, and a composite has no band count of its own.
TEST(mod_data, alpha_band_is_not_checked_against_a_single_band)
{
    const auto my_root = std::string(IMG_DATA_TEST_SOURCE_DIR);
    const auto pgm = rsvp::ImageData::read(
        my_root + "/unit_test_data/image_data/hemisphere.pgm");
    ASSERT_TRUE(pgm != nullptr);
    ASSERT_EQ(pgm->get_bands(), 1);

    std::list<std::string> tokens = {"[", "alpha_band", "2", "]"};
    EXPECT_TRUE(rsvp::apply_properties(pgm, &tokens, "test.mod"));
    EXPECT_EQ(pgm->get_alpha_band(), 2);

    const auto composite =
        std::make_shared<rsvp::AlphaBlendingCompositeData>();
    composite->add_image(pgm);
    composite->add_image(rsvp::VicarData::read_vicarfile(
        my_root +
        "/unit_test_data/image_data/NLB_530659343RASLF0582340NCAM00385M1.ht"));
    ASSERT_EQ(composite->get_bands(), 1);

    tokens = {"[", "alpha_band", "2", "]"};
    EXPECT_TRUE(rsvp::apply_properties(composite, &tokens, "test.mod"));

    // The composite still blends: the PGM as opaque and the heightmap by
    // its own alpha band
    double value = 0.0;
    EXPECT_NO_THROW(
        composite->get_interpolated_pixel_double(value, 1.0, 1.0, 0));
}
