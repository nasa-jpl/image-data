# Make Utils Paths Library
set(IMG_DATA_TEST_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(UTILS_SOURCES
    test_utils/test_utils.h
    test_utils/test_utils.cc
)

set(DEPENDENCY_LIBRARIES
    pthread
)

# Define abs path to image_data/test in header file
configure_file(Config.h.in ${CMAKE_CURRENT_BINARY_DIR}/Config.h)

# Tests
add_library(image_data_test_utils ${UTILS_SOURCES})
add_library(img_data_gtest img_data_gtest/gtest.cc)

add_executable(
    test_image_data
    test_unit.cc
    test_unit_composite.cc
    test_unit_image_data.cc
    test_interpolation.cc
    test_image_data_colors.cc
    test_file_detection.cc
    test_z_offset_data.cc
    test_composite_data.cc
    test_inline_methods.cc
)

target_link_libraries(
    test_image_data
    PRIVATE
        image_data
        img_data_gtest
        image_data_test_utils
        ${DEPENDENCY_LIBRARIES}
)

# Include gtest lib
target_include_directories(test_image_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Include Config.h which contains an abs path to image_data/test src
target_include_directories(test_image_data PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
