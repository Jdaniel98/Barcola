#include <gtest/gtest.h>

#include <cmath>

// Guards the -ffp-contract=off in the top-level CMakeLists.txt. The golden outputs can't: they
// print 10 significant digits, and match with contraction on or off.
//
// With x = 1 + 2^-27, x*x is exactly 1 + 2^-26 + 2^-54. A double can't hold the last term, so
// the product rounds to 1 + 2^-26, and subtracting that gives exactly 0. A fused multiply-subtract
// rounds only once, at the end, and gives 2^-54. On a machine without FMA this always passes; CI's
// Ubuntu and macOS builds both have it. The volatiles stop the compiler working it out itself.

namespace {

const double X = 1.0 + std::ldexp(1.0, -27);
const double X_SQUARED_ROUNDED = 1.0 + std::ldexp(1.0, -26);

}  // namespace

TEST(FpContractionTest, MultiplySubtractInOneStatement_RoundsTwice) {
    const volatile double x = X;
    const volatile double y = X_SQUARED_ROUNDED;
    const double a = x;
    const double b = y;
    const double result = a * a - b;
    EXPECT_EQ(result, 0.0);
}

// GCC's default, -ffp-contract=fast, also fuses across statements
TEST(FpContractionTest, MultiplySubtractAcrossStatements_RoundsTwice) {
    const volatile double x = X;
    const volatile double y = X_SQUARED_ROUNDED;
    const double a = x;
    const double b = y;
    const double product = a * a;
    const double result = product - b;
    EXPECT_EQ(result, 0.0);
}

// The shape of the z-score's sumSqDiff += diff * diff
TEST(FpContractionTest, RunningSumOfSquares_RoundsTwice) {
    const volatile double x = X;
    const volatile double y = X_SQUARED_ROUNDED;
    const double diff = x;
    double sum = -y;
    sum += diff * diff;
    EXPECT_EQ(sum, 0.0);
}
