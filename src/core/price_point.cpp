#include "core/price_point.h"

#include <iostream>
#include <sstream>

#include "core/time_utils.h"

namespace barcola {

PricePoint::PricePoint(time_t date, double opening, double highest, double lowest, double closing)
    : date_(date), opening_(opening), highest_(highest), lowest_(lowest), closing_(closing) {}

PricePoint::PricePoint(const std::string& date, double opening, double highest, double lowest,
                       double closing)
    : date_(convertDateToEpoch(date)),
      opening_(opening),
      highest_(highest),
      lowest_(lowest),
      closing_(closing) {}

PricePoint::PricePoint(time_t date, double price)
    : date_(date), opening_(price), highest_(price), lowest_(price), closing_(price) {}

PricePoint::PricePoint(const std::string& date, double price)
    : date_(convertDateToEpoch(date)),
      opening_(price),
      highest_(price),
      lowest_(price),
      closing_(price) {}

time_t PricePoint::getDate() const {
    return date_;
}

std::string PricePoint::getDateString() const {
    return convertEpochToDate(date_);
}

double PricePoint::getOpening() const {
    return opening_;
}

double PricePoint::getHighest() const {
    return highest_;
}

double PricePoint::getLowest() const {
    return lowest_;
}

double PricePoint::getClosing() const {
    return closing_;
}

std::string PricePoint::toString() const {
    std::ostringstream oss;
    oss << "{ date: " << getDateString() << " opening: " << opening_ << " highest: " << highest_
        << " lowest: " << lowest_ << " closing: " << closing_ << " }";
    return oss.str();
}

void PricePoint::printPricePoint() const {
    std::cout << toString() << std::endl;
}

}  // namespace barcola
