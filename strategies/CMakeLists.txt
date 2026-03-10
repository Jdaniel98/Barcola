add_library(barcola_strategies STATIC
    mean_reversion_strategy.cpp
)

target_include_directories(barcola_strategies PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(barcola_strategies PUBLIC barcola)
