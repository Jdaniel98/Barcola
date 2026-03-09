#include <gtest/gtest.h>
#include "core/price_point.h"
#include "core/time_utils.h"

using namespace barcola;

TEST(PricePointTest, ConstructWithOHLC) {
    PricePoint pp("2023-01-03", 130.28, 130.90, 124.17, 125.07);
    EXPECT_DOUBLE_EQ(pp.getOpening(), 130.28);
    EXPECT_DOUBLE_EQ(pp.getHighest(), 130.90);
    EXPECT_DOUBLE_EQ(pp.getLowest(), 124.17);
    EXPECT_DOUBLE_EQ(pp.getClosing(), 125.07);
    EXPECT_EQ(pp.getDateString(), "2023-01-03");
}

TEST(PricePointTest, ConstructWithSinglePrice) {
    PricePoint pp("2023-01-03", 150.0);
    EXPECT_DOUBLE_EQ(pp.getOpening(), 150.0);
    EXPECT_DOUBLE_EQ(pp.getHighest(), 150.0);
    EXPECT_DOUBLE_EQ(pp.getLowest(), 150.0);
    EXPECT_DOUBLE_EQ(pp.getClosing(), 150.0);
}

TEST(PricePointTest, ConstructWithEpoch) {
    time_t epoch = convertDateToEpoch("2023-06-15");
    PricePoint pp(epoch, 100.0, 110.0, 90.0, 105.0);
    EXPECT_EQ(pp.getDateString(), "2023-06-15");
    EXPECT_DOUBLE_EQ(pp.getClosing(), 105.0);
}

TEST(PricePointTest, ToString_ContainsExpectedFields) {
    PricePoint pp("2023-01-03", 130.28, 130.90, 124.17, 125.07);
    std::string str = pp.toString();
    EXPECT_NE(str.find("2023-01-03"), std::string::npos);
    EXPECT_NE(str.find("opening"), std::string::npos);
    EXPECT_NE(str.find("closing"), std::string::npos);
}
