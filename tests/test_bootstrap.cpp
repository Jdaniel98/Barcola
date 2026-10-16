#include <gtest/gtest.h>
#include "analysis/bootstrap.h"

using namespace barcola;

TEST(BootstrapTest, Indices_LengthAndRange) {
    std::mt19937 generator(1);
    const auto indices = blockBootstrapIndices(100, 21, generator);
    ASSERT_EQ(indices.size(), 100u);
    for (size_t i : indices) {
        EXPECT_LT(i, 100u);
    }
}

TEST(BootstrapTest, Indices_ComeInConsecutiveBlocks) {
    std::mt19937 generator(2);
    const auto indices = blockBootstrapIndices(100, 10, generator);
    // Within each block of 10 the indices count up by one
    for (size_t block = 0; block < 10; ++block) {
        for (size_t k = 1; k < 10; ++k) {
            EXPECT_EQ(indices[block * 10 + k], indices[block * 10 + k - 1] + 1);
        }
    }
}

TEST(BootstrapTest, Indices_LastBlockIsCut) {
    std::mt19937 generator(3);
    EXPECT_EQ(blockBootstrapIndices(25, 10, generator).size(), 25u);
}

TEST(BootstrapTest, Indices_SameSeedSameResult) {
    std::mt19937 a(7);
    std::mt19937 b(7);
    EXPECT_EQ(blockBootstrapIndices(500, 21, a), blockBootstrapIndices(500, 21, b));
}

TEST(BootstrapTest, Indices_BlockAsLongAsSeriesReturnsIdentity) {
    std::mt19937 generator(4);
    const auto indices = blockBootstrapIndices(5, 5, generator);
    EXPECT_EQ(indices, (std::vector<size_t>{0, 1, 2, 3, 4}));
}

TEST(BootstrapTest, Indices_InvalidBlock_Throws) {
    std::mt19937 generator(5);
    EXPECT_THROW((void)blockBootstrapIndices(10, 0, generator), std::invalid_argument);
    EXPECT_THROW((void)blockBootstrapIndices(10, 11, generator), std::invalid_argument);
}

TEST(BootstrapTest, Percentile_KnownValues) {
    const std::vector<double> values = {5.0, 1.0, 4.0, 2.0, 3.0};  // unsorted on purpose
    EXPECT_DOUBLE_EQ(percentile(values, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(percentile(values, 0.5), 3.0);
    EXPECT_DOUBLE_EQ(percentile(values, 1.0), 5.0);
    EXPECT_DOUBLE_EQ(percentile(values, 0.125), 1.5);  // halfway between ranks 0 and 1
}

TEST(BootstrapTest, Percentile_InvalidInput_Throws) {
    EXPECT_THROW((void)percentile({}, 0.5), std::invalid_argument);
    EXPECT_THROW((void)percentile({1.0}, 1.5), std::invalid_argument);
}
