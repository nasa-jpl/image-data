# `image-data`

`image-data` is a lightweight and efficient C++17 library for parsing and
writing VICAR image files, a format widely used by Jet Propulsion Laboratory
(JPL) on missions such as Mars 2020 and the Mars Science Laboratory. It is
designed to address the challenge of decoding the VICAR format, which is
commonly used for both internal mission data and publicly archived imagery in
the Planetary Data System (PDS). The library is small, portable, and optimized
for fast decoding, providing a modern and maintainable solution for working
with mission-critical image data using the C++17 standard.

## Build & Run tests
```bash
mkdir build
cd build
cmake ..
make
```

Run unit tests from build directory:
```bash
./test/test_image_data
```
