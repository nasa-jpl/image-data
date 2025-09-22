#ifndef RSVP_IMAGE_DATA_TEST_UTILS_H
#define RSVP_IMAGE_DATA_TEST_UTILS_H

#include <string>

namespace image_data
{
    std::string image_data_test_mkdtemp(const std::string &templatefilename);

    int image_data_test_rm_directory(const std::string &dir);
}

#endif // TEST_UTILS_H