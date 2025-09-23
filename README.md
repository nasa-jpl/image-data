# `image-data`

`image-data` is a lightweight and efficient C++17 library for parsing and
writing VICAR image files, a format widely used by Jet Propulsion Laboratory
(JPL) on missions such as Mars 2020 and the Mars Science Laboratory. It is
designed to address the challenge of decoding the VICAR format, which is
commonly used for both internal mission data and publicly archived imagery in
the [Planetary Data System
(PDS)](https://pds-imaging.jpl.nasa.gov/beta/archive-explorer?mission=mars_2020).
The library is small, portable, and optimized for fast decoding, providing a
modern and maintainable solution for working with mission-critical image data
using the C++17 standard.

## Usage

The example below parses
[`NLF_1497_0799838646_034EDR_N0730000NCAM00709_01_095J01.IMG`](https://pds-imaging.jpl.nasa.gov/archive/m20/r13/mars2020_navcam_ops_raw/data/sol/01497/ids/edr/ncam/NLF_1497_0799838646_034EDR_N0730000NCAM00709_01_095J01.IMG)
(and assumes it has been downloaded to the current directory already):

```cpp
#include <image_data.h>
#include <vicar_data.h>

#include <iostream>
#include <memory>


int main()
{
    // Read a VICAR file using the static factory method.
    std::shared_ptr<rsvp::ImageData> image = rsvp::ImageData::read(
        "NLF_1497_0799838646_034EDR_N0730000NCAM00709_01_095J01.IMG");
    if (not image)
    {
        std::cerr << "Failed to read image file" << std::endl;
        return 1;
    }

    // Get image dimensions.
    std::cout << "Image dimensions: " << image->get_width() << "x"
              << image->get_height() << "x" << image->get_bands()
              << std::endl;

    // Access pixel data (first pixel, first band).
    double pixel_value;
    if (image->get_pixel_double(pixel_value, 0, 0, 0))
    {
        std::cout << "Pixel value at (0,0,0): " << pixel_value << std::endl;
    }

    // Cast to VicarData for format-specific operations.
    const auto vicar_image = std::dynamic_pointer_cast<rsvp::VicarData>(image);
    if (vicar_image)
    {
        // Access VICAR-specific metadata.
        std::cout << "VICAR data organization: "
                  << (vicar_image->get_org() == rsvp::VicarData::BSQ
                          ? "Band Sequential"
                          : vicar_image->get_org() == rsvp::VicarData::BIL
                                ? "Band Interleaved by Line"
                                : "Band Interleaved by Pixel")
                  << std::endl;

        // Get label property if available.
        std::string property_value;
        if (vicar_image->get_label_property(
                "IDENTIFICATION", "MISSION_NAME", property_value))
        {
            std::cout << "Mission: " << property_value << std::endl;
        }
    }
}
```

## Build
```bash
mkdir build
cd build
cmake ..
make
```

## Test
Run unit tests from build directory:
```bash
./test/test_image_data
```

## CMake integration

Here's an example of integrating image-data into your CMake project as a
subdirectory:

```cmake
# Add the image-data repository as a subdirectory
add_subdirectory(path/to/image-data)

# Create your application
add_executable(your_app main.cc)

# Link against the image_data library
target_link_libraries(your_app PRIVATE image_data)
```
