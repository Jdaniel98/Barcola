#pragma once

#include <cstddef>
#include <random>
#include <vector>

namespace barcola {

// Indices for one moving-block bootstrap resample of a series of length n: consecutive runs
// of blockLength indices, each starting at a uniformly drawn position, cut to exactly n.
// Blocks keep short-range dependence (volatility clustering) that resampling single days
// would destroy. Positions are drawn with generator() % range rather than a standard
// distribution, so the same seed gives the same indices with every compiler.
// Throws std::invalid_argument unless 0 < blockLength <= n.
[[nodiscard]] std::vector<size_t> blockBootstrapIndices(size_t n, size_t blockLength,
                                                        std::mt19937& generator);

// The value below which `fraction` of the values fall, interpolating linearly between the
// two closest ranks. Throws std::invalid_argument for no values or a fraction outside [0, 1].
[[nodiscard]] double percentile(std::vector<double> values, double fraction);

}  // namespace barcola
