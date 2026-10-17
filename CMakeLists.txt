cmake_minimum_required(VERSION 3.16)
project(Barcola VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)

option(BARCOLA_WARNINGS_AS_ERRORS "Treat compiler warnings in Barcola's code as errors" OFF)
option(BARCOLA_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BARCOLA_COVERAGE "Instrument the build for gcov test coverage" OFF)

# Everything is instrumented, so sanitizer and coverage builds also cover GoogleTest's
# code paths; any undefined behavior aborts the test that triggered it.
if(BARCOLA_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(BARCOLA_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Warnings for Barcola's own targets only; GoogleTest keeps its own settings
add_library(barcola_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(barcola_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        $<$<BOOL:${BARCOLA_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

add_subdirectory(src)
add_subdirectory(strategies)

option(BARCOLA_BUILD_APPS "Build the study programs in apps/" ON)
if(BARCOLA_BUILD_APPS)
    add_subdirectory(apps)
endif()

option(BARCOLA_BUILD_TESTS "Build tests" ON)
if(BARCOLA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
