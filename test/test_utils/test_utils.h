#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <string>

namespace image_data
{
    std::string image_data_test_mkdtemp(const std::string &templatefilename);

    int image_data_test_rm_directory(const std::string &dir);
}

#endif // TEST_UTILS_H