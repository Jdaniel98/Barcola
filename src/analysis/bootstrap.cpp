#include "analysis/bootstrap.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace barcola {

std::vector<size_t> blockBootstrapIndices(size_t n, size_t blockLength,
                                          std::mt19937& generator) {
    if (blockLength == 0 || blockLength > n) {
        throw std::invalid_argument("Block length must be between 1 and the series length");
    }
    const size_t starts = n - blockLength + 1;
    std::vector<size_t> indices;
    indices.reserve(n + blockLength);
    while (indices.size() < n) {
        const size_t start = static_cast<size_t>(generator()) % starts;
        for (size_t i = 0; i < blockLength; ++i) {
            indices.push_back(start + i);
        }
    }
    indices.resize(n);
    return indices;
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        throw std::invalid_argument("percentile needs at least one value");
    }
    if (!(fraction >= 0.0 && fraction <= 1.0)) {
        throw std::invalid_argument("percentile fraction must be in [0, 1]");
    }
    std::sort(values.begin(), values.end());
    const double position = fraction * static_cast<double>(values.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(position));
    const size_t upper = std::min(lower + 1, values.size() - 1);
    const double weight = position - static_cast<double>(lower);
    return values[lower] + weight * (values[upper] - values[lower]);
}

}  // namespace barcola
