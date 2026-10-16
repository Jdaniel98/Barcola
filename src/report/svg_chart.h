#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace barcola {

// Static SVG charts for the study write-ups. Each chart is a self-contained SVG with its own
// light and dark color scheme (switched by prefers-color-scheme), so it renders correctly as
// an image on GitHub.

// Which color a series gets. Colors follow the entity: a strategy keeps its color in every
// chart. Benchmark is a neutral gray for context series.
enum class SeriesColor { First, Second, Benchmark };

enum class ValueFormat {
    Decimal,   // 0.67
    Percent,   // −24% (from -0.24)
    Currency,  // $125k
};

// Axis range and tick positions on round numbers (1, 2 or 5 times a power of ten).
struct NiceScale {
    double min = 0.0;
    double max = 0.0;
    double step = 0.0;
    std::vector<double> ticks;
};

// Covers [dataMin, dataMax] with at most about maxTicks ticks.
// Throws std::invalid_argument if dataMin > dataMax or maxTicks < 2.
[[nodiscard]] NiceScale niceScale(double dataMin, double dataMax, size_t maxTicks = 6);

[[nodiscard]] std::string formatValue(double value, ValueFormat format);

// Escapes &, <, >, " and ' for SVG text and attributes.
[[nodiscard]] std::string escapeXml(const std::string& text);

struct LineSeries {
    std::string name;
    SeriesColor color = SeriesColor::First;
    std::vector<double> values;
};

// Lines over time. dates holds one YYYY-MM-DD string per point; the x axis is labelled at
// the first point of each year. Each line ends in a dot with its last value.
struct LineChart {
    std::string title;
    std::string subtitle;
    std::vector<std::string> dates;
    std::vector<LineSeries> series;
    ValueFormat format = ValueFormat::Decimal;
    std::optional<double> forceMax;  // e.g. 0 for drawdowns, so the top of the axis is 0
};

// Small multiples: one panel per series, stacked, sharing both axes. Each series is a line
// with a light wash between it and zero, and its lowest point is marked and labelled with
// its value and month. Suited to drawdowns, where overlaid lines would tangle.
struct PanelChart {
    std::string title;
    std::string subtitle;
    std::vector<std::string> dates;  // one YYYY-MM-DD per point
    std::vector<LineSeries> series;
    ValueFormat format = ValueFormat::Percent;
    std::string lowestLabel = "worst";
};

// One row per item: a hollow dot for the "from" value, a filled dot for the "to" value.
struct DumbbellRow {
    std::string label;
    SeriesColor color = SeriesColor::First;
    double from = 0.0;
    double to = 0.0;
};

struct DumbbellChart {
    std::string title;
    std::string subtitle;
    std::string fromLabel;
    std::string toLabel;
    std::vector<DumbbellRow> rows;
    ValueFormat format = ValueFormat::Decimal;
};

// Dots along one value axis, one row per group, with an optional vertical reference line.
struct StripRow {
    std::string label;
    SeriesColor color = SeriesColor::First;
    std::vector<double> values;
    std::optional<size_t> highlighted;  // index into values, ringed and labelled
    std::string highlightLabel;
};

struct ReferenceLine {
    std::string label;
    double value = 0.0;
};

struct StripChart {
    std::string title;
    std::string subtitle;
    std::string axisLabel;
    std::vector<StripRow> rows;
    std::optional<ReferenceLine> reference;
    ValueFormat format = ValueFormat::Decimal;
};

// Each renderer returns a complete SVG document.
// Throws std::invalid_argument on empty input or mismatched series lengths.
[[nodiscard]] std::string renderLineChart(const LineChart& chart);
[[nodiscard]] std::string renderPanelChart(const PanelChart& chart);
[[nodiscard]] std::string renderDumbbellChart(const DumbbellChart& chart);
[[nodiscard]] std::string renderStripChart(const StripChart& chart);

}  // namespace barcola
