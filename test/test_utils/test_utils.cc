#include "test_utils.h"

#include <libgen.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>

#include <cstdlib>
#include <unistd.h>

#define MAX_TEMPLATE_LENGTH 256

namespace image_data
{

    std::string image_data_test_mkdtemp(const std::string &templatefilename)
    {
        if (templatefilename.length() >= MAX_TEMPLATE_LENGTH)
        {
            std::cerr << "image_data_test_mkdtemp(): template filename length must be < "
                    << MAX_TEMPLATE_LENGTH << std::endl;
            return std::string();
        }

        char buf[MAX_TEMPLATE_LENGTH];
        snprintf(buf, MAX_TEMPLATE_LENGTH - 1, "%s", templatefilename.c_str());

        char *dir = mkdtemp(buf);
        if (dir == nullptr)
        {
            return std::string();
        }

        return std::string(dir);
    }

    int image_data_test_rm_directory(const std::string &dir)
    {
        std::stringstream cmd;
        cmd << "rm -r '" << dir << "'";
        return system(cmd.str().c_str());
    }
}
