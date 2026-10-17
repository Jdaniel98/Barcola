#include <gtest/gtest.h>
#include "report/svg_chart.h"

using namespace barcola;

static size_t countOf(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t pos = haystack.find(needle); pos != std::string::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

// --- niceScale ---

TEST(SvgChartTest, NiceScale_RoundRange) {
    const NiceScale s = niceScale(0.0, 100.0, 6);
    EXPECT_DOUBLE_EQ(s.min, 0.0);
    EXPECT_DOUBLE_EQ(s.max, 100.0);
    EXPECT_DOUBLE_EQ(s.step, 20.0);
    ASSERT_EQ(s.ticks.size(), 6u);
    EXPECT_DOUBLE_EQ(s.ticks[1], 20.0);
}

TEST(SvgChartTest, NiceScale_SpansNegativeValues) {
    // Range 0.75 rounds up to 1; 1 / 5 = 0.2 steps; -0.07 floors to -0.2, 0.68 ceils to 0.8
    const NiceScale s = niceScale(-0.07, 0.68, 6);
    EXPECT_NEAR(s.min, -0.2, 1e-12);
    EXPECT_NEAR(s.max, 0.8, 1e-12);
    EXPECT_NEAR(s.step, 0.2, 1e-12);
    ASSERT_EQ(s.ticks.size(), 6u);
    EXPECT_NEAR(s.ticks[1], 0.0, 1e-12);
}

TEST(SvgChartTest, NiceScale_CoversData) {
    const NiceScale s = niceScale(93412.0, 216877.0, 6);
    EXPECT_LE(s.min, 93412.0);
    EXPECT_GE(s.max, 216877.0);
    EXPECT_LE(s.ticks.size(), 8u);
}

TEST(SvgChartTest, NiceScale_EqualMinMax_StillHasRange) {
    const NiceScale s = niceScale(5.0, 5.0, 6);
    EXPECT_LT(s.min, 5.0);
    EXPECT_GT(s.max, 5.0);
}

TEST(SvgChartTest, NiceScale_InvalidInput_Throws) {
    EXPECT_THROW((void)niceScale(2.0, 1.0, 6), std::invalid_argument);
    EXPECT_THROW((void)niceScale(0.0, 1.0, 1), std::invalid_argument);
}

// --- formatting ---

TEST(SvgChartTest, FormatValue) {
    EXPECT_EQ(formatValue(0.672, ValueFormat::Decimal), "0.67");
    EXPECT_EQ(formatValue(-0.07, ValueFormat::Decimal), "\xE2\x88\x92" "0.07");
    EXPECT_EQ(formatValue(-0.24, ValueFormat::Percent), "\xE2\x88\x92" "24%");
    EXPECT_EQ(formatValue(-0.238, ValueFormat::Percent), "\xE2\x88\x92" "23.8%");
    EXPECT_EQ(formatValue(0.0, ValueFormat::Percent), "0%");
    EXPECT_EQ(formatValue(125300.0, ValueFormat::Currency), "$125k");
    EXPECT_EQ(formatValue(1340000.0, ValueFormat::Currency), "$1.3M");
    EXPECT_EQ(formatValue(950.0, ValueFormat::Currency), "$950");
}

TEST(SvgChartTest, EscapeXml) {
    EXPECT_EQ(escapeXml("a<b & \"c\" 'd'>"), "a&lt;b &amp; &quot;c&quot; &apos;d&apos;&gt;");
}

// --- renderers ---

TEST(SvgChartTest, LineChart_OnePolylinePerSeries) {
    LineChart chart;
    chart.title = "Growth <test>";
    chart.dates = {"2020-01-02", "2020-06-01", "2021-01-04"};
    chart.series = {{"A", SeriesColor::First, {1.0, 2.0, 3.0}},
                    {"B", SeriesColor::Benchmark, {1.0, 1.5, 2.0}}};
    const std::string svg = renderLineChart(chart);

    EXPECT_EQ(svg.rfind("<svg", 0), 0u);
    EXPECT_EQ(countOf(svg, "</svg>"), 1u);
    EXPECT_EQ(countOf(svg, "<polyline"), 2u);
    EXPECT_NE(svg.find("Growth &lt;test&gt;"), std::string::npos);
    EXPECT_NE(svg.find(">2020<"), std::string::npos);
    EXPECT_NE(svg.find(">2021<"), std::string::npos);
    EXPECT_NE(svg.find("prefers-color-scheme:dark"), std::string::npos);
}

TEST(SvgChartTest, LineChart_MismatchedSeries_Throws) {
    LineChart chart;
    chart.dates = {"2020-01-02", "2020-01-03"};
    chart.series = {{"A", SeriesColor::First, {1.0}}};
    EXPECT_THROW((void)renderLineChart(chart), std::invalid_argument);

    LineChart empty;
    empty.dates = {"2020-01-02", "2020-01-03"};
    EXPECT_THROW((void)renderLineChart(empty), std::invalid_argument);
}

TEST(SvgChartTest, DumbbellChart_TwoDotsPerRow) {
    DumbbellChart chart;
    chart.fromLabel = "Before";
    chart.toLabel = "After";
    chart.rows = {{"Mean reversion", SeriesColor::First, 0.75, 0.30},
                  {"Crossover", SeriesColor::Second, 0.59, 0.65}};
    const std::string svg = renderDumbbellChart(chart);

    // Two legend glyphs plus two dots per row
    EXPECT_EQ(countOf(svg, "<circle"), 2u + 4u);
    EXPECT_NE(svg.find(">0.75<"), std::string::npos);
    EXPECT_NE(svg.find(">0.30<"), std::string::npos);
    EXPECT_NE(svg.find(">Mean reversion<"), std::string::npos);
}

TEST(SvgChartTest, StripChart_DotsReferenceAndHighlight) {
    StripChart chart;
    chart.rows = {{"MR", SeriesColor::First, {0.1, 0.3, 0.3, 0.2}, 1, "chosen"},
                  {"MA", SeriesColor::Second, {0.5, 0.6}, std::nullopt, ""}};
    chart.reference = ReferenceLine{"Buy-and-hold", 0.67};
    const std::string svg = renderStripChart(chart);

    // Two legend dots + six data dots + one highlight ring
    EXPECT_EQ(countOf(svg, "<circle"), 2u + 6u + 1u);
    EXPECT_EQ(countOf(svg, "class=\"highlight\""), 1u);
    EXPECT_NE(svg.find(">Buy-and-hold 0.67<"), std::string::npos);
    EXPECT_NE(svg.find(">chosen<"), std::string::npos);
}

TEST(SvgChartTest, StripChart_OverlappingDotsAreSeparated) {
    StripChart chart;
    chart.rows = {{"MR", SeriesColor::First, {0.3, 0.3, 0.3}, std::nullopt, ""}};
    const std::string svg = renderStripChart(chart);

    // Identical values get three different vertical positions
    std::vector<std::string> cys;
    for (size_t pos = svg.find("class=\"ring d1\""); pos != std::string::npos;
         pos = svg.find("class=\"ring d1\"", pos + 1)) {
        const size_t start = svg.rfind("cy=\"", pos) + 4;
        cys.push_back(svg.substr(start, svg.find('"', start) - start));
    }
    ASSERT_EQ(cys.size(), 3u);
    EXPECT_NE(cys[0], cys[1]);
    EXPECT_NE(cys[1], cys[2]);
    EXPECT_NE(cys[0], cys[2]);
}

TEST(SvgChartTest, PanelChart_OnePanelPerSeriesWithLowestPointLabelled) {
    PanelChart chart;
    chart.dates = {"2020-01-02", "2020-03-16", "2020-06-01"};
    chart.series = {{"A", SeriesColor::First, {0.0, -0.34, -0.05}},
                    {"B", SeriesColor::Benchmark, {0.0, -0.10, 0.0}}};
    const std::string svg = renderPanelChart(chart);

    EXPECT_EQ(countOf(svg, "<polyline"), 2u);
    EXPECT_EQ(countOf(svg, "<polygon"), 2u);
    EXPECT_NE(svg.find(">worst \xE2\x88\x92" "34%, Mar 2020<"), std::string::npos);
    EXPECT_NE(svg.find(">worst \xE2\x88\x92" "10%, Mar 2020<"), std::string::npos);
}

TEST(SvgChartTest, PanelChart_MismatchedSeries_Throws) {
    PanelChart chart;
    chart.dates = {"2020-01-02", "2020-01-03"};
    chart.series = {{"A", SeriesColor::First, {0.0, -0.1, -0.2}}};
    EXPECT_THROW((void)renderPanelChart(chart), std::invalid_argument);
}

TEST(SvgChartTest, LineChart_ShortSeriesGetADotPerPoint) {
    LineChart chart;
    chart.dates = {"2016-07-01", "2017-07-01", "2018-07-01"};
    chart.series = {{"A", SeriesColor::First, {0.5, 1.0, 0.2}}};
    // Three dots per series: two on the line plus the end dot
    EXPECT_EQ(countOf(renderLineChart(chart), "class=\"ring d1\""), 3u);
}

TEST(SvgChartTest, LineChart_LongSeriesOnlyEndDot) {
    LineChart chart;
    for (int i = 0; i < 100; ++i) {
        chart.dates.push_back("2020-01-01");
    }
    chart.series = {{"A", SeriesColor::First, std::vector<double>(100, 1.0)}};
    EXPECT_EQ(countOf(renderLineChart(chart), "class=\"ring d1\""), 1u);
}

TEST(SvgChartTest, LineChart_FourSeriesColors) {
    LineChart chart;
    chart.dates = {"2025-01-02", "2025-06-02", "2026-01-02"};
    chart.series = {{"A", SeriesColor::First, {1, 2, 3}},
                    {"B", SeriesColor::Second, {1, 2, 3}},
                    {"C", SeriesColor::Third, {1, 2, 3}},
                    {"D", SeriesColor::Fourth, {1, 2, 3}}};
    const std::string svg = renderLineChart(chart);
    for (const char* cls : {"line c1", "line c2", "line c3", "line c4"}) {
        EXPECT_NE(svg.find(cls), std::string::npos) << cls;
    }
    // Every color has a light and a dark value
    EXPECT_EQ(countOf(svg, ".c3{"), 2u);
    EXPECT_EQ(countOf(svg, ".c4{"), 2u);
}
