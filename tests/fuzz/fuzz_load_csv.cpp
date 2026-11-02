// libFuzzer target for PriceHistory::loadFromCsv, Barcola's only input from outside the program.
// Malformed input must raise std::runtime_error or std::invalid_argument; anything else,
// including any other exception, is a bug and crashes the run.
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "core/price_history.h"

namespace {

// loadFromCsv takes only a path, so each input goes through this file. One per process, removed
// when the process exits normally.
class TempCsv {
public:
    TempCsv()
        : path_(std::filesystem::temp_directory_path() /
                ("barcola_fuzz_" + std::to_string(getpid()) + ".csv")) {}
    TempCsv(const TempCsv&) = delete;
    TempCsv& operator=(const TempCsv&) = delete;
    ~TempCsv() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    // Aborts if the input can't be written. Otherwise each run would look like an ordinary
    // load error, and the job would pass having fuzzed nothing.
    [[nodiscard]] std::string write(const uint8_t* data, size_t size) const {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.close();
        if (!file) {
            std::abort();
        }
        return path_.string();
    }

private:
    std::filesystem::path path_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const TempCsv csv;
    const std::string path = csv.write(data, size);
    barcola::PriceHistory history("FUZZ");
    try {
        history.loadFromCsv(path);
    } catch (const std::runtime_error&) {
        return 0;  // Expected for malformed input
    } catch (const std::invalid_argument&) {
        return 0;  // Expected for malformed input
    }
    return 0;
}
