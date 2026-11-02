include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
)
FetchContent_MakeAvailable(googletest)

function(barcola_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE barcola barcola_warnings GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(${name} PRIVATE
        BARCOLA_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

barcola_add_test(test_time_utils)
barcola_add_test(test_price_point)
barcola_add_test(test_price_history)
barcola_add_test(test_moving_average)
barcola_add_test(test_rsi)
barcola_add_test(test_bollinger)
barcola_add_test(test_correlation)
barcola_add_test(test_mean_reversion)
barcola_add_test(test_market_data)
barcola_add_test(test_position_sizing)
barcola_add_test(test_slippage)
barcola_add_test(test_monte_carlo)
barcola_add_test(test_csv_io)
barcola_add_test(test_svg_chart)
barcola_add_test(test_bootstrap)
barcola_add_test(test_fp_contraction)

# Phase 2: backtest tests need strategies library too
function(barcola_add_backtest_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE barcola_strategies barcola_warnings GTest::gtest_main)
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/strategies)
    target_compile_definitions(${name} PRIVATE
        BARCOLA_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

barcola_add_test(test_performance)
barcola_add_backtest_test(test_backtest_engine)
barcola_add_backtest_test(test_mean_reversion_strategy)
barcola_add_backtest_test(test_buy_and_hold_strategy)
barcola_add_backtest_test(test_ma_crossover_strategy)
barcola_add_backtest_test(test_window_stats)

# study_common is part of the study programs, so its test compiles it directly
add_executable(test_study_common test_study_common.cpp ${PROJECT_SOURCE_DIR}/apps/study_common.cpp)
target_link_libraries(test_study_common PRIVATE barcola_strategies barcola_warnings GTest::gtest_main)
target_include_directories(test_study_common PRIVATE ${PROJECT_SOURCE_DIR}/apps)
add_test(NAME test_study_common COMMAND test_study_common)

# The top-level CMakeLists.txt refuses flags that relax floating-point arithmetic. Configuring
# stops before anything is built or downloaded.
function(barcola_add_refused_flag_test name setting flag)
    add_test(NAME ${name}
             COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}
                     -B ${CMAKE_CURRENT_BINARY_DIR}/${name}
                     -G ${CMAKE_GENERATOR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                     -DBARCOLA_BUILD_TESTS=OFF -DBARCOLA_BUILD_APPS=OFF "-D${setting}=${flag}")
    set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "contains ${flag}")
endfunction()
barcola_add_refused_flag_test(refuses_fast_math CMAKE_CXX_FLAGS -ffast-math)
barcola_add_refused_flag_test(refuses_ofast_in_release CMAKE_CXX_FLAGS_RELEASE -Ofast)

# Golden outputs: both study programs on the synthetic prices in data/golden, compared with the
# committed outputs in tests/golden. They run again with the margin report on, which must change
# no result file. These need Python, and the study programs for all but the first test.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_compare_golden
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/test_compare_golden.py)

    if(BARCOLA_BUILD_APPS)
        set(golden_run ${PROJECT_SOURCE_DIR}/scripts/run_golden.sh ${CMAKE_BINARY_DIR})
        set(golden_compare ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/scripts/compare_golden.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/golden)
        set(golden_out ${CMAKE_CURRENT_BINARY_DIR}/golden_out)
        set(golden_out_margins ${CMAKE_CURRENT_BINARY_DIR}/golden_out_margins)
        set(golden_margins ${CMAKE_CURRENT_BINARY_DIR}/golden_margins.csv)

        add_test(NAME golden_run COMMAND ${golden_run} ${golden_out})
        add_test(NAME golden_outputs COMMAND ${golden_compare} ${golden_out})
        set_tests_properties(golden_run PROPERTIES FIXTURES_SETUP golden_run)
        set_tests_properties(golden_outputs PROPERTIES FIXTURES_REQUIRED golden_run)

        # The report keeps rows already in its file, so each run starts from a clean one
        add_test(NAME golden_margins_clean COMMAND ${CMAKE_COMMAND} -E remove -f ${golden_margins})
        add_test(NAME golden_run_with_margin_report
                 COMMAND ${CMAKE_COMMAND} -E env BARCOLA_MARGIN_REPORT=${golden_margins}
                         ${golden_run} ${golden_out_margins})
        add_test(NAME golden_outputs_with_margin_report
                 COMMAND ${golden_compare} ${golden_out_margins})
        add_test(NAME golden_margin_report
                 COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/check_margin_report.py
                         ${golden_margins})
        set_tests_properties(golden_margins_clean golden_run_with_margin_report
                             PROPERTIES FIXTURES_SETUP golden_run_margins)
        set_tests_properties(golden_run_with_margin_report
                             PROPERTIES DEPENDS golden_margins_clean)
        set_tests_properties(golden_outputs_with_margin_report golden_margin_report
                             PROPERTIES FIXTURES_REQUIRED golden_run_margins)
    endif()
endif()
