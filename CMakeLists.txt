cmake_minimum_required(VERSION 3.13)

project(image_data_proj)

option(IMAGE_DATA_BUILD_TESTING "Whether to build image_data tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Treat warnings as errors on compiler versions we regularly test on.
if(CMAKE_COMPILER_IS_GNUCXX)
    message(STATUS "GCC compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "8.5.0")
        message(STATUS "Treating warnings as error on known compiler version")
        # Disable `-Werror only when fuzzing`
        if(NOT FUZZING_BUILD)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
        endif()
    endif()
endif()

add_library(
    image_data
    composite_data.cc
    composite_data.h
    csv_data.cc
    csv_data.h
    image_data.cc
    image_data.h
    mod_data.cc
    mod_data.h
    pgm_data.cc
    pgm_data.h
    translated_data.cc
    translated_data.h
    vicar_data.cc
    vicar_data.h
    z_offset_data.cc
    z_offset_data.h
)

target_include_directories(image_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(
    image_data
    PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -pedantic)

if(IMAGE_DATA_BUILD_TESTING)
    add_subdirectory(test)
endif()
