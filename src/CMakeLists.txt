add_library(barcola STATIC
    core/time_utils.cpp
    core/price_point.cpp
    core/price_history.cpp
    indicators/moving_average.cpp
    indicators/rsi.cpp
    indicators/bollinger_bands.cpp
    analysis/market_data_analysis.cpp
    analysis/correlation.cpp
    analysis/mean_reversion.cpp
    risk/position_sizing.cpp
    risk/slippage_model.cpp
    risk/dynamic_hedging.cpp
    simulation/monte_carlo.cpp
    backtest/performance.cpp
    backtest/backtest_engine.cpp
)

target_include_directories(barcola PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(barcola PUBLIC CURL::libcurl)
