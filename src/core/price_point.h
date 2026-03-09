#pragma once

#include <ctime>
#include <string>

namespace barcola {

class PricePoint {
public:
    PricePoint(time_t date, double opening, double highest, double lowest, double closing);
    PricePoint(const std::string& date, double opening, double highest, double lowest,
               double closing);
    PricePoint(time_t date, double price);
    PricePoint(const std::string& date, double price);
    ~PricePoint() = default;

    [[nodiscard]] time_t getDate() const;
    [[nodiscard]] std::string getDateString() const;
    [[nodiscard]] double getOpening() const;
    [[nodiscard]] double getHighest() const;
    [[nodiscard]] double getLowest() const;
    [[nodiscard]] double getClosing() const;

    [[nodiscard]] std::string toString() const;
    void printPricePoint() const;

private:
    time_t date_;
    double opening_;
    double highest_;
    double lowest_;
    double closing_;
};

}  // namespace barcola
