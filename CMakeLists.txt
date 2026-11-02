cmake_minimum_required(VERSION 3.16)
project(Barcola VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BARCOLA_WARNINGS_AS_ERRORS "Treat compiler warnings in Barcola's code as errors" OFF)
option(BARCOLA_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BARCOLA_COVERAGE "Instrument the build for gcov test coverage" OFF)
option(BARCOLA_FUZZ "Build the libFuzzer target in tests/fuzz (needs LLVM Clang)" OFF)

# Results must be the same on every platform, so a*b + c stays two rounded operations rather
# than one fused multiply-add. This covers every target; GoogleTest is unaffected in practice.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-ffp-contract=off)
endif()

# These flags let the compiler reorder and approximate floating-point arithmetic.
set(barcola_cxx_flag_vars CMAKE_CXX_FLAGS)
foreach(config Debug Release RelWithDebInfo MinSizeRel ${CMAKE_BUILD_TYPE}
               ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    list(APPEND barcola_cxx_flag_vars "CMAKE_CXX_FLAGS_${config_upper}")
endforeach()
foreach(flag_var IN LISTS barcola_cxx_flag_vars)
    foreach(flag -ffast-math -Ofast -fassociative-math -funsafe-math-optimizations)
        string(FIND "${${flag_var}}" "${flag}" flag_position)
        if(NOT flag_position EQUAL -1)
            message(FATAL_ERROR "${flag_var} contains ${flag}, which would make results depend "
                                "on the compiler. Barcola's results must be reproducible.")
        endif()
    endforeach()
endforeach()

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

if(BARCOLA_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "BARCOLA_FUZZ needs LLVM Clang, which includes libFuzzer; found "
                            "${CMAKE_CXX_COMPILER_ID}. Apple clang does not include it.")
    endif()
    add_subdirectory(tests/fuzz)
endif()
