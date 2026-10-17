#include "report/svg_chart.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace barcola {

namespace {

constexpr double WIDTH = 720.0;
constexpr double TITLE_Y = 30.0;
constexpr double SUBTITLE_Y = 50.0;
constexpr double LEGEND_Y = 76.0;
constexpr double PLOT_TOP = 104.0;
constexpr double FONT_SIZE = 12.0;
constexpr double CHAR_WIDTH = 0.58 * FONT_SIZE;  // rough average glyph width for layout
constexpr double DOT_RADIUS = 5.0;
constexpr const char* MINUS = "\xE2\x88\x92";  // U+2212, a proper minus sign

// Colors from the validated reference palette: series slots 1 and 2, a neutral gray for the
// benchmark, and ink/gridline tokens. Dark mode uses its own steps, not an inversion.
constexpr const char* STYLE = R"(<style>
text{font-family:system-ui,-apple-system,"Segoe UI",Helvetica,Arial,sans-serif}
.bg{fill:#fcfcfb;stroke:rgba(11,11,11,0.10)}
.title{fill:#0b0b0b;font-size:16px;font-weight:600}
.sub{fill:#52514e;font-size:13px}
.ink{fill:#0b0b0b;font-size:12px}
.ink2{fill:#52514e;font-size:12px}
.tick{fill:#898781;font-size:12px;font-variant-numeric:tabular-nums}
.grid{stroke:#e1e0d9;stroke-width:1}
.axis{stroke:#c3c2b7;stroke-width:1}
.line{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.c1{stroke:#2a78d6}.d1{fill:#2a78d6}
.c2{stroke:#eb6834}.d2{fill:#eb6834}
.c3{stroke:#1baf7a}.d3{fill:#1baf7a}
.c4{stroke:#eda100}.d4{fill:#eda100}
.cb{stroke:#898781}.db{fill:#898781}
.ring{stroke:#fcfcfb;stroke-width:2}
.hollow{fill:#fcfcfb;stroke-width:2}
.glyph{fill:#52514e;stroke:#52514e}
.glyphStroke{stroke:#52514e}
.a1{fill:#2a78d6;fill-opacity:0.12}
.a2{fill:#eb6834;fill-opacity:0.12}
.a3{fill:#1baf7a;fill-opacity:0.12}
.a4{fill:#eda100;fill-opacity:0.12}
.ab{fill:#898781;fill-opacity:0.14}
.connector{stroke:#898781;stroke-width:2}
.leader{stroke:#898781;stroke-width:1}
.ref{stroke:#52514e;stroke-width:1.5}
.highlight{fill:none;stroke:#0b0b0b;stroke-width:1.5}
@media (prefers-color-scheme:dark){
.bg{fill:#1a1a19;stroke:rgba(255,255,255,0.10)}
.title,.ink{fill:#ffffff}
.sub,.ink2{fill:#c3c2b7}
.grid{stroke:#2c2c2a}
.axis{stroke:#383835}
.c1{stroke:#3987e5}.d1{fill:#3987e5}
.c2{stroke:#d95926}.d2{fill:#d95926}
.c3{stroke:#199e70}.d3{fill:#199e70}
.c4{stroke:#c98500}.d4{fill:#c98500}
.ring{stroke:#1a1a19}
.hollow{fill:#1a1a19}
.glyph{fill:#c3c2b7;stroke:#c3c2b7}
.glyphStroke{stroke:#c3c2b7}
.a1{fill:#3987e5;fill-opacity:0.16}
.a2{fill:#d95926;fill-opacity:0.16}
.a3{fill:#199e70;fill-opacity:0.16}
.a4{fill:#c98500;fill-opacity:0.16}
.ref{stroke:#c3c2b7}
.highlight{stroke:#ffffff}
}
</style>
)";

std::string colorKey(SeriesColor color) {
    switch (color) {
        case SeriesColor::First:
            return "1";
        case SeriesColor::Second:
            return "2";
        case SeriesColor::Third:
            return "3";
        case SeriesColor::Fourth:
            return "4";
        case SeriesColor::Benchmark:
            return "b";
    }
    return "b";
}

std::string num(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

double textWidth(const std::string& text) {
    // Count UTF-8 code points, not bytes
    const auto isLeadByte = [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; };
    const double chars = static_cast<double>(std::count_if(text.begin(), text.end(), isLeadByte));
    return chars * CHAR_WIDTH;
}

// Maps a value in [scale.min, scale.max] onto [from, to] pixels.
double project(const NiceScale& scale, double value, double from, double to) {
    return from + (value - scale.min) / (scale.max - scale.min) * (to - from);
}

std::string text(double x, double y, const std::string& cls, const std::string& content,
                 const char* anchor = "start") {
    std::ostringstream out;
    out << "<text x=\"" << num(x) << "\" y=\"" << num(y) << "\" class=\"" << cls
        << "\" text-anchor=\"" << anchor << "\">" << escapeXml(content) << "</text>\n";
    return out.str();
}

std::string line(double x1, double y1, double x2, double y2, const std::string& cls) {
    std::ostringstream out;
    out << "<line x1=\"" << num(x1) << "\" y1=\"" << num(y1) << "\" x2=\"" << num(x2) << "\" y2=\""
        << num(y2) << "\" class=\"" << cls << "\"/>\n";
    return out.str();
}

std::string circle(double x, double y, double r, const std::string& cls) {
    std::ostringstream out;
    out << "<circle cx=\"" << num(x) << "\" cy=\"" << num(y) << "\" r=\"" << num(r) << "\" class=\""
        << cls << "\"/>\n";
    return out.str();
}

std::string header(double height, const std::string& title, const std::string& subtitle) {
    std::ostringstream out;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num(WIDTH) << "\" height=\""
        << num(height) << "\" viewBox=\"0 0 " << num(WIDTH) << " " << num(height)
        << "\" role=\"img\" aria-labelledby=\"chart-title chart-desc\">\n"
        << "<title id=\"chart-title\">" << escapeXml(title) << "</title>\n"
        << "<desc id=\"chart-desc\">" << escapeXml(subtitle) << "</desc>\n"
        << STYLE << "<rect x=\"0.5\" y=\"0.5\" width=\"" << num(WIDTH - 1) << "\" height=\""
        << num(height - 1) << "\" rx=\"8\" class=\"bg\"/>\n"
        << text(24, TITLE_Y, "title", title) << text(24, SUBTITLE_Y, "sub", subtitle);
    return out.str();
}

enum class LegendGlyph { Line, Dot, HollowDot };

struct LegendItem {
    std::string label;
    std::string glyphClass;  // e.g. "c1" for a line or "d1" for a dot
    LegendGlyph glyph;
};

double glyphWidth(LegendGlyph glyph) {
    return glyph == LegendGlyph::Line ? 22.0 : 16.0;
}

// Row of each legend item: items flow left to right and wrap when the next would pass the
// chart's right margin.
std::vector<size_t> legendRows(const std::vector<LegendItem>& items) {
    constexpr double LEFT = 24.0;
    std::vector<size_t> rows;
    double x = LEFT;
    size_t row = 0;
    for (const auto& item : items) {
        const double width = glyphWidth(item.glyph) + textWidth(item.label);
        if (x > LEFT && x + width > WIDTH - LEFT) {
            ++row;
            x = LEFT;
        }
        rows.push_back(row);
        x += width + 24;
    }
    return rows;
}

constexpr double LEGEND_ROW_HEIGHT = 20.0;

// Extra height a wrapped legend needs; everything below it moves down by this much.
double legendExtraHeight(const std::vector<LegendItem>& items) {
    const std::vector<size_t> rows = legendRows(items);
    return rows.empty() ? 0.0 : static_cast<double>(rows.back()) * LEGEND_ROW_HEIGHT;
}

std::string legend(const std::vector<LegendItem>& items) {
    const std::vector<size_t> rows = legendRows(items);
    std::string out;
    double x = 24.0;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (i > 0 && rows[i] != rows[i - 1]) {
            x = 24.0;
        }
        const double y = LEGEND_Y + static_cast<double>(rows[i]) * LEGEND_ROW_HEIGHT;
        switch (item.glyph) {
            case LegendGlyph::Line:
                out += line(x, y - 4, x + 16, y - 4, "line " + item.glyphClass);
                break;
            case LegendGlyph::Dot:
                out += circle(x + 5, y - 4, DOT_RADIUS, item.glyphClass);
                break;
            case LegendGlyph::HollowDot:
                out += circle(x + 5, y - 4, DOT_RADIUS - 1, "hollow " + item.glyphClass);
                break;
        }
        x += glyphWidth(item.glyph);
        out += text(x, y, "ink2", item.label);
        x += textWidth(item.label) + 24;
    }
    return out;
}

// Vertical gridlines and tick labels for a horizontal value axis.
std::string valueAxisX(const NiceScale& scale, double left, double right, double top, double bottom,
                       ValueFormat format) {
    std::string out;
    for (double tick : scale.ticks) {
        const double x = project(scale, tick, left, right);
        out += line(x, top, x, bottom, "grid");
        out += text(x, bottom + 18, "tick", formatValue(tick, format), "middle");
    }
    return out;
}

void requireNonEmpty(bool empty, const char* what) {
    if (empty) {
        throw std::invalid_argument(std::string("Chart has no ") + what);
    }
}

}  // namespace

NiceScale niceScale(double dataMin, double dataMax, size_t maxTicks) {
    if (!(dataMin <= dataMax) || !std::isfinite(dataMin) || !std::isfinite(dataMax)) {
        throw std::invalid_argument("niceScale needs finite dataMin <= dataMax");
    }
    if (maxTicks < 2) {
        throw std::invalid_argument("niceScale needs at least 2 ticks");
    }
    if (dataMax - dataMin < 1e-12) {
        const double pad = std::max(std::abs(dataMin) * 0.1, 1.0);
        dataMin -= pad;
        dataMax += pad;
    }

    // Heckbert's "nice numbers": 1, 2 or 5 times a power of ten
    const auto nice = [](double range, bool round) {
        const double exponent = std::floor(std::log10(range));
        const double fraction = range / std::pow(10.0, exponent);
        double niceFraction = 10.0;
        if (round) {
            niceFraction = fraction < 1.5   ? 1.0
                           : fraction < 3.0 ? 2.0
                           : fraction < 7.0 ? 5.0
                                            : 10.0;
        } else {
            niceFraction = fraction <= 1.0   ? 1.0
                           : fraction <= 2.0 ? 2.0
                           : fraction <= 5.0 ? 5.0
                                             : 10.0;
        }
        return niceFraction * std::pow(10.0, exponent);
    };

    NiceScale scale;
    const double range = nice(dataMax - dataMin, false);
    scale.step = nice(range / static_cast<double>(maxTicks - 1), true);
    scale.min = std::floor(dataMin / scale.step) * scale.step;
    scale.max = std::ceil(dataMax / scale.step) * scale.step;
    // Count in whole steps, so no rounding accumulates and 0.30000000000000004 prints as 0.3
    const auto firstStep = static_cast<long long>(std::round(scale.min / scale.step));
    const auto lastStep = static_cast<long long>(std::round(scale.max / scale.step));
    for (long long k = firstStep; k <= lastStep; ++k) {
        scale.ticks.push_back(static_cast<double>(k) * scale.step);
    }
    return scale;
}

std::string formatValue(double value, ValueFormat format) {
    std::ostringstream out;
    const bool negative = value < 0.0 && std::abs(value) > 1e-12;
    const double magnitude = std::abs(value);
    if (negative) {
        out << MINUS;
    }
    switch (format) {
        case ValueFormat::Decimal:
            out << std::fixed << std::setprecision(2) << magnitude;
            break;
        case ValueFormat::Percent: {
            const double percent = magnitude * 100.0;
            const bool whole = std::abs(percent - std::round(percent)) < 1e-6;
            out << std::fixed << std::setprecision(whole ? 0 : 1) << percent << "%";
            break;
        }
        case ValueFormat::Currency:
            if (magnitude >= 1e6) {
                out << "$" << std::fixed << std::setprecision(1) << magnitude / 1e6 << "M";
            } else if (magnitude >= 1e3) {
                out << "$" << std::fixed << std::setprecision(0) << magnitude / 1e3 << "k";
            } else {
                out << "$" << std::fixed << std::setprecision(0) << magnitude;
            }
            break;
    }
    return out.str();
}

std::string escapeXml(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

std::string renderLineChart(const LineChart& chart) {
    requireNonEmpty(chart.series.empty(), "series");
    const size_t n = chart.dates.size();
    if (n < 2) {
        throw std::invalid_argument("Line chart needs at least two dates");
    }
    double lo = chart.series.front().values.empty() ? 0.0 : chart.series.front().values.front();
    double hi = lo;
    for (const auto& s : chart.series) {
        if (s.values.size() != n) {
            throw std::invalid_argument("Series '" + s.name + "' does not match the dates");
        }
        const auto [mn, mx] = std::minmax_element(s.values.begin(), s.values.end());
        lo = std::min(lo, *mn);
        hi = std::max(hi, *mx);
    }
    if (chart.forceMax) {
        hi = std::max(hi, *chart.forceMax);
    }

    const NiceScale scale = niceScale(lo, hi, 6);
    std::vector<LegendItem> items;
    for (const auto& s : chart.series) {
        items.push_back({s.name, "c" + colorKey(s.color), LegendGlyph::Line});
    }
    const double left = 72.0;
    const double right = WIDTH - 96.0;
    const double top = PLOT_TOP + legendExtraHeight(items);
    const double bottom = top + 260.0;
    const double height = bottom + 40.0;
    const auto x = [&](size_t i) {
        return left + static_cast<double>(i) / static_cast<double>(n - 1) * (right - left);
    };
    const auto y = [&](double v) { return project(scale, v, bottom, top); };

    std::ostringstream out;
    out << header(height, chart.title, chart.subtitle) << legend(items);

    for (double tick : scale.ticks) {
        out << line(left, y(tick), right, y(tick), "grid")
            << text(left - 8, y(tick) + 4, "tick", formatValue(tick, chart.format), "end");
    }
    out << line(left, bottom, right, bottom, "axis");

    // Year labels at the first point of each year
    std::string lastYear;
    for (size_t i = 0; i < n; ++i) {
        const std::string year = chart.dates[i].substr(0, 4);
        if (year != lastYear) {
            out << text(x(i), bottom + 18, "tick", year, "middle");
            lastYear = year;
        }
    }

    // Benchmark lines first so the strategies draw on top
    std::vector<const LineSeries*> order;
    for (const auto& s : chart.series) {
        if (s.color == SeriesColor::Benchmark)
            order.push_back(&s);
    }
    for (const auto& s : chart.series) {
        if (s.color != SeriesColor::Benchmark)
            order.push_back(&s);
    }
    for (const LineSeries* s : order) {
        out << "<polyline class=\"line c" << colorKey(s->color) << "\" points=\"";
        for (size_t i = 0; i < n; ++i) {
            out << (i == 0 ? "" : " ") << num(x(i)) << "," << num(y(s->values[i]));
        }
        out << "\"/>\n";
    }

    // Short series (e.g. one point per year) get a dot on every point so each value is visible
    constexpr size_t MAX_POINTS_WITH_DOTS = 40;
    if (n <= MAX_POINTS_WITH_DOTS) {
        for (const LineSeries* s : order) {
            for (size_t i = 0; i + 1 < n; ++i) {
                out << circle(x(i), y(s->values[i]), DOT_RADIUS - 1, "ring d" + colorKey(s->color));
            }
        }
    }

    // End dots and value labels; labels that would collide are moved apart and get a leader
    struct EndLabel {
        double dotY;
        double labelY;
        std::string value;
    };
    std::vector<EndLabel> labels;
    for (const LineSeries* s : order) {
        const double endY = y(s->values.back());
        out << circle(right, endY, DOT_RADIUS - 1, "ring d" + colorKey(s->color));
        labels.push_back({endY, endY, formatValue(s->values.back(), chart.format)});
    }
    std::sort(labels.begin(), labels.end(),
              [](const EndLabel& a, const EndLabel& b) { return a.dotY < b.dotY; });
    constexpr double LABEL_GAP = 16.0;
    for (size_t i = 1; i < labels.size(); ++i) {
        labels[i].labelY = std::max(labels[i].labelY, labels[i - 1].labelY + LABEL_GAP);
    }
    for (const auto& label : labels) {
        const double labelX = right + 14;
        if (std::abs(label.labelY - label.dotY) > 0.5) {
            out << line(right + 5, label.dotY, labelX - 3, label.labelY - 4, "leader");
        }
        out << text(labelX, label.labelY + 4, "ink", label.value);
    }

    out << "</svg>\n";
    return out.str();
}

std::string renderPanelChart(const PanelChart& chart) {
    requireNonEmpty(chart.series.empty(), "series");
    const size_t n = chart.dates.size();
    if (n < 2) {
        throw std::invalid_argument("Panel chart needs at least two dates");
    }
    double lo = 0.0;
    double hi = 0.0;
    for (const auto& s : chart.series) {
        if (s.values.size() != n) {
            throw std::invalid_argument("Series '" + s.name + "' does not match the dates");
        }
        const auto [mn, mx] = std::minmax_element(s.values.begin(), s.values.end());
        lo = std::min(lo, *mn);
        hi = std::max(hi, *mx);
    }

    const NiceScale scale = niceScale(lo, hi, 3);
    constexpr double PANEL_HEIGHT = 96.0;
    constexpr double PANEL_STRIDE = PANEL_HEIGHT + 52.0;  // panel, a gap and its name above it
    const double left = 72.0;
    const double right = WIDTH - 48.0;
    const double firstTop = PLOT_TOP - 8.0;
    const double lastBottom =
        firstTop + PANEL_STRIDE * static_cast<double>(chart.series.size() - 1) + PANEL_HEIGHT;
    const double height = lastBottom + 32.0;
    const auto x = [&](size_t i) {
        return left + static_cast<double>(i) / static_cast<double>(n - 1) * (right - left);
    };

    std::ostringstream out;
    out << header(height, chart.title, chart.subtitle);

    for (size_t p = 0; p < chart.series.size(); ++p) {
        const LineSeries& s = chart.series[p];
        const std::string key = colorKey(s.color);
        const double top = firstTop + PANEL_STRIDE * static_cast<double>(p);
        const double bottom = top + PANEL_HEIGHT;
        const auto y = [&](double v) { return project(scale, v, bottom, top); };

        // Panel name with a short line key in the series color
        out << line(left, top - 14, left + 16, top - 14, "line c" + key)
            << text(left + 22, top - 10, "ink", s.name);

        for (double tick : scale.ticks) {
            out << line(left, y(tick), right, y(tick), "grid")
                << text(left - 8, y(tick) + 4, "tick", formatValue(tick, chart.format), "end");
        }

        // Wash between the line and zero, then the line itself
        out << "<polygon class=\"a" << key << "\" points=\"" << num(x(0)) << "," << num(y(0.0));
        for (size_t i = 0; i < n; ++i) {
            out << " " << num(x(i)) << "," << num(y(s.values[i]));
        }
        out << " " << num(x(n - 1)) << "," << num(y(0.0)) << "\"/>\n"
            << "<polyline class=\"line c" << key << "\" points=\"";
        for (size_t i = 0; i < n; ++i) {
            out << (i == 0 ? "" : " ") << num(x(i)) << "," << num(y(s.values[i]));
        }
        out << "\"/>\n";

        // Mark the lowest point and label it with its value and month
        const size_t lowest = static_cast<size_t>(
            std::min_element(s.values.begin(), s.values.end()) - s.values.begin());
        static const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        const int month = std::stoi(chart.dates[lowest].substr(5, 2));
        const std::string label = chart.lowestLabel + " " +
                                  formatValue(s.values[lowest], chart.format) + ", " +
                                  MONTHS[month - 1] + " " + chart.dates[lowest].substr(0, 4);
        const double lowX = x(lowest);
        const double lowY = y(s.values[lowest]);
        const bool roomRight = lowX + 10 + textWidth(label) < right;
        out << circle(lowX, lowY, DOT_RADIUS - 1, "ring d" + key)
            << text(lowX + (roomRight ? 10 : -10), lowY + 4, "ink", label,
                    roomRight ? "start" : "end");
    }

    // Year labels under the last panel
    std::string lastYear;
    for (size_t i = 0; i < n; ++i) {
        const std::string year = chart.dates[i].substr(0, 4);
        if (year != lastYear) {
            out << text(x(i), lastBottom + 18, "tick", year, "middle");
            lastYear = year;
        }
    }

    out << "</svg>\n";
    return out.str();
}

std::string renderDumbbellChart(const DumbbellChart& chart) {
    requireNonEmpty(chart.rows.empty(), "rows");
    double lo = 0.0;
    double hi = 0.0;
    for (const auto& row : chart.rows) {
        lo = std::min({lo, row.from, row.to});
        hi = std::max({hi, row.from, row.to});
    }

    const NiceScale scale = niceScale(lo, hi, 6);
    constexpr double ROW_HEIGHT = 48.0;
    const double left = 190.0;
    const double right = WIDTH - 48.0;
    const double top = PLOT_TOP;
    const double bottom = top + ROW_HEIGHT * static_cast<double>(chart.rows.size());
    const double height = bottom + 40.0;
    const auto x = [&](double v) { return project(scale, v, left, right); };

    std::ostringstream out;
    out << header(height, chart.title, chart.subtitle)
        << legend({{chart.fromLabel, "glyphStroke", LegendGlyph::HollowDot},
                   {chart.toLabel, "glyph", LegendGlyph::Dot}})
        << valueAxisX(scale, left, right, top, bottom, chart.format)
        << line(x(0.0), top, x(0.0), bottom, "axis");

    for (size_t i = 0; i < chart.rows.size(); ++i) {
        const auto& row = chart.rows[i];
        const double rowY = top + ROW_HEIGHT * (static_cast<double>(i) + 0.5);
        const std::string key = colorKey(row.color);
        const double fromX = x(row.from);
        const double toX = x(row.to);
        out << text(left - 16, rowY + 4, "ink", row.label, "end")
            << line(fromX, rowY, toX, rowY, "connector")
            << circle(fromX, rowY, DOT_RADIUS, "hollow c" + key)
            << circle(toX, rowY, DOT_RADIUS + 1, "ring d" + key);
        // Each value sits on the outside of its dot, away from the other one
        const bool rising = row.to >= row.from;
        out << text(fromX + (rising ? -12 : 12), rowY + 4, "ink2",
                    formatValue(row.from, chart.format), rising ? "end" : "start")
            << text(toX + (rising ? 12 : -12), rowY + 4, "ink", formatValue(row.to, chart.format),
                    rising ? "start" : "end");
    }

    out << "</svg>\n";
    return out.str();
}

std::string renderStripChart(const StripChart& chart) {
    requireNonEmpty(chart.rows.empty(), "rows");
    double lo = 0.0;
    double hi = 0.0;
    for (const auto& row : chart.rows) {
        requireNonEmpty(row.values.empty(), "values in a row");
        const auto [mn, mx] = std::minmax_element(row.values.begin(), row.values.end());
        lo = std::min(lo, *mn);
        hi = std::max(hi, *mx);
    }
    if (chart.reference) {
        lo = std::min(lo, chart.reference->value);
        hi = std::max(hi, chart.reference->value);
    }

    const NiceScale scale = niceScale(lo, hi, 6);
    constexpr double ROW_HEIGHT = 92.0;
    const double left = 190.0;
    const double right = WIDTH - 48.0;
    std::vector<LegendItem> items;
    for (const auto& row : chart.rows) {
        items.push_back({row.label, "d" + colorKey(row.color), LegendGlyph::Dot});
    }
    const double top = PLOT_TOP + 20.0 + legendExtraHeight(items);  // room for the reference label
    const double bottom = top + ROW_HEIGHT * static_cast<double>(chart.rows.size());
    const double height = bottom + 60.0;
    const auto x = [&](double v) { return project(scale, v, left, right); };

    std::ostringstream out;
    out << header(height, chart.title, chart.subtitle);
    out << legend(items) << valueAxisX(scale, left, right, top, bottom, chart.format)
        << line(left, bottom, right, bottom, "axis")
        << text((left + right) / 2.0, bottom + 40, "tick", chart.axisLabel, "middle");

    if (chart.reference) {
        const double refX = x(chart.reference->value);
        const std::string label =
            chart.reference->label + " " + formatValue(chart.reference->value, chart.format);
        out << line(refX, top - 6, refX, bottom, "ref")
            << text(refX, top - 12, "ink2", label,
                    refX + textWidth(label) / 2.0 > WIDTH - 8 ? "end" : "middle");
    }

    for (size_t r = 0; r < chart.rows.size(); ++r) {
        const auto& row = chart.rows[r];
        const double rowY = top + ROW_HEIGHT * (static_cast<double>(r) + 0.5);
        out << text(left - 16, rowY + 4, "ink", row.label, "end");

        // Place dots left to right; a dot that would overlap one already placed moves to the
        // nearest free vertical offset (a simple beeswarm).
        std::vector<size_t> byValue(row.values.size());
        for (size_t i = 0; i < byValue.size(); ++i)
            byValue[i] = i;
        std::sort(byValue.begin(), byValue.end(),
                  [&](size_t a, size_t b) { return row.values[a] < row.values[b]; });
        constexpr double OFFSETS[] = {0, -11, 11, -22, 22};
        constexpr double MIN_DISTANCE = 2.0 * DOT_RADIUS + 1.0;
        std::vector<std::pair<double, double>> placed;
        std::vector<double> dotY(row.values.size(), rowY);
        for (size_t i : byValue) {
            const double dotX = x(row.values[i]);
            for (double offset : OFFSETS) {
                const bool free = std::none_of(placed.begin(), placed.end(), [&](const auto& p) {
                    return std::hypot(p.first - dotX, p.second - (rowY + offset)) < MIN_DISTANCE;
                });
                if (free) {
                    dotY[i] = rowY + offset;
                    break;
                }
            }
            placed.emplace_back(dotX, dotY[i]);
            out << circle(dotX, dotY[i], DOT_RADIUS, "ring d" + colorKey(row.color));
        }

        if (row.highlighted && *row.highlighted < row.values.size()) {
            const size_t h = *row.highlighted;
            const double hx = x(row.values[h]);
            // The label goes below the whole cluster so it never sits on other dots
            out << circle(hx, dotY[h], DOT_RADIUS + 4, "highlight");
            if (!row.highlightLabel.empty()) {
                // Keep the label off the reference line: if the line would cross it, set the
                // label beside the line instead of centred under the dot.
                const double half = textWidth(row.highlightLabel) / 2.0;
                double labelX = hx;
                const char* anchor = "middle";
                if (chart.reference) {
                    const double refX = x(chart.reference->value);
                    if (refX > hx - half - 4 && refX < hx + half + 4) {
                        labelX = refX >= hx ? refX - 6 : refX + 6;
                        anchor = refX >= hx ? "end" : "start";
                    }
                }
                const double labelY = rowY + ROW_HEIGHT / 2.0 - 4;
                out << text(labelX, labelY, "ink2", row.highlightLabel, anchor);
            }
        }
    }

    out << "</svg>\n";
    return out.str();
}

}  // namespace barcola
