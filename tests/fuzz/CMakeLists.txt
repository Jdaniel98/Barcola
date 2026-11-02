# libFuzzer only learns from code built with its instrumentation, so the target compiles the CSV
# loader's sources itself instead of linking the barcola library. The sanitizer flags stay on
# this target only.
add_executable(fuzz_load_csv
    fuzz_load_csv.cpp
    ${PROJECT_SOURCE_DIR}/src/core/price_history.cpp
    ${PROJECT_SOURCE_DIR}/src/core/price_point.cpp
    ${PROJECT_SOURCE_DIR}/src/core/time_utils.cpp
)
target_include_directories(fuzz_load_csv PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(fuzz_load_csv PRIVATE
    -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
target_link_options(fuzz_load_csv PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_libraries(fuzz_load_csv PRIVATE barcola_warnings)
