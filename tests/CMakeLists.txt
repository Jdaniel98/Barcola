include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

function(barcola_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE barcola GTest::gtest_main)
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
barcola_add_test(test_dynamic_hedging)
barcola_add_test(test_monte_carlo)
barcola_add_test(test_csv_io)

# Phase 2: backtest tests need strategies library too
function(barcola_add_backtest_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE barcola barcola_strategies GTest::gtest_main)
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
