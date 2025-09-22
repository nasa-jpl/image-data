#include <image_data.h>
#include <platform.h>

#include <img_data_gtest/gtest.h>
#include <test_utils/test_utils.h>

#include "Config.h"

#include <fstream>

TEST(image_data_file_detection, read_unrecognized_extension)
{
    // Create a test file with unrecognized extension
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);
    std::string filename = tmp_dir + "/test.unknown";

    std::ofstream ofs;
    ofs.open(filename, std::ofstream::trunc);
    ofs << "test data" << std::endl;
    ofs.close();

    EXPECT_THROW(rsvp::ImageData::read(filename), std::runtime_error);

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}

TEST(image_data_file_detection, read_non_existent_file)
{
    EXPECT_THROW(rsvp::ImageData::read("/non/existent/file.mod"),
                 std::runtime_error);
}

TEST(image_data_file_detection, read_different_case_extensions)
{
    // Create a test file with uppercase MOD extension
    std::string tmp_dir =
        image_data::image_data_test_mkdtemp("/tmp/tmp.XXXXXX");
    ASSERT_TRUE(tmp_dir.length() != 0);

    // Test different extensions
    const std::vector<std::string> extensions = {
        "mod",
        "MOD",
        "mod_tc",
        "MOD_TC", // For ModData
        "img",
        "IMG",
        "vic",
        "VIC", // For VicarData
        "ht",
        "HT",
        "tc",
        "TC", // For ModData bare vicar
        "csv",
        "CSV", // For CSVData
        "pgm",
        "PGM" // For PGMData
    };

    for (const auto &ext : extensions)
    {
        std::string filename = tmp_dir + "/test." + ext;
        std::ofstream ofs;
        ofs.open(filename, std::ofstream::trunc);
        // Write minimal content to prevent immediate parsing failure
        // Note: These may still throw due to content format issues
        // but that's not what we're testing here
        if (ext == "mod" || ext == "MOD" || ext == "mod_tc" || ext == "MOD_TC")
        {
            ofs << "I~" << std::endl;
        }
        else if (ext == "pgm" || ext == "PGM")
        {
            ofs << "P5" << std::endl
                << "1 1" << std::endl
                << "255" << std::endl
                << "X";
        }
        else if (ext == "csv" || ext == "CSV")
        {
            ofs << "1,1" << std::endl;
        }
        else
        {
            ofs << "LBLSIZE=1024" << std::endl;
        }
        ofs.close();

        // We don't check if it succeeds, just that the extension is recognized
        // and it doesn't throw the "Unrecognized file type" exception
        try
        {
            rsvp::ImageData::read(filename);
        }
        catch (const std::runtime_error &e)
        {
            const std::string error_msg = e.what();
            EXPECT_EQ(error_msg.find("Unrecognized file type"),
                      std::string::npos)
                << "Extension " << ext << " not recognized: " << error_msg;
        }
    }

    ASSERT_TRUE(image_data::image_data_test_rm_directory(tmp_dir) == 0);
}