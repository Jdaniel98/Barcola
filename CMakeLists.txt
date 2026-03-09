cmake_minimum_required(VERSION 3.16)
project(Barcola VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)

add_subdirectory(src)

option(BARCOLA_BUILD_TESTS "Build tests" ON)
if(BARCOLA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
