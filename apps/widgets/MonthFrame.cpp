#include "MonthFrame.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include <Arduino.h>
#include <epd_driver.h>

#include "core/RenderContext.h"
#include "firasans.h"
#include "Layout.h"
#include "ScaledText.h"

namespace apps {
namespace widgets {

namespace {

constexpr const char* kWeekdayLabels[] = {"Po", "Ut", "St", "Ct", "Pa", "So", "Ne"};
constexpr const char* kMonthNames[] = {
    "Leden",    "Unor",    "Brezen",   "Duben", "Kveten",   "Cerven",
    "Cervenec", "Srpen",   "Zari",     "Rijen", "Listopad", "Prosinec"};
constexpr const char* kMonthAbbrev[] = {
    "Led", "Uno", "Bre", "Dub", "Kve", "Cvn", "Cvc", "Srp", "Zar", "Rij", "Lis", "Pro"};
constexpr uint8_t kShadingColor = 180;
constexpr int32_t kShadingSpacing = 8;

void drawTextLine(const GFXfont* font, const std::string& text, int32_t x, int32_t y, uint8_t* pixels) {
    if (!font || !pixels || text.empty()) {
        return;
    }

    int32_t drawX = x;
    int32_t drawY = y;
    writeln((GFXfont*)font, text.c_str(), &drawX, &drawY, pixels);
}

int32_t floorDiv2(int32_t value) {
    if (value >= 0) {
        return value / 2;
    }
    return -(((-value) + 1) / 2);
}

void drawCenteredTextInCell(const GFXfont* font,
                            const std::string& text,
                            int32_t cellX,
                            int32_t cellY,
                            int32_t cellWidth,
                            int32_t cellHeight,
                            uint8_t* pixels) {
    if (!font || !pixels || text.empty() || cellWidth <= 0 || cellHeight <= 0) {
        return;
    }

    const int32_t padding = 4;
    const int32_t availableWidth = std::max<int32_t>(0, cellWidth - padding * 2);
    const bool useHalfScale = measureTextWidth(font, text) > availableWidth;

    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t w = 0;
    int32_t h = 0;
    get_text_bounds(font, text.c_str(), &x, &y, &x1, &y1, &w, &h, nullptr);

    if (w <= 0 || h <= 0) {
        return;
    }

    int32_t drawW = w;
    int32_t drawX1 = x1;
    int32_t asc = font->ascender;
    int32_t desc = font->descender;

    if (useHalfScale) {
        drawW = std::max<int32_t>(1, (w + 1) / 2);
        drawX1 = floorDiv2(x1);
        asc = floorDiv2(asc);
        desc = floorDiv2(desc);
    }

    const int32_t centerX = cellX + cellWidth / 2;
    const int32_t centerY = cellY + cellHeight / 2;
    const int32_t targetLeftX = centerX - drawW / 2;
    const int32_t baselineX = targetLeftX - drawX1;
    const int32_t baselineY = centerY + (asc + desc) / 2;

    if (useHalfScale) {
        drawTextHalf(font, text, baselineX, baselineY, pixels);
    } else {
        drawTextLine(font, text, baselineX, baselineY, pixels);
    }
}

void drawDiagonalShading(int32_t originX, int32_t originY, int32_t cellWidth, int32_t cellHeight, uint8_t* pixels) {
    if (!pixels || cellWidth <= 2 || cellHeight <= 2) {
        return;
    }

    const int32_t maxLocalX = cellWidth - 1;
    const int32_t maxLocalY = cellHeight - 1;
    const int32_t minAbsY = originY;
    const int32_t maxAbsY = originY + cellHeight - 1;

    struct Point {
        int32_t x;
        int32_t y;
    };

    auto drawThickLine = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t offsetSign) {
        epd_draw_line(x0, y0, x1, y1, kShadingColor, pixels);
        const int32_t y0b = y0 + offsetSign;
        const int32_t y1b = y1 + offsetSign;
        if (y0b >= minAbsY && y0b <= maxAbsY && y1b >= minAbsY && y1b <= maxAbsY) {
            epd_draw_line(x0, y0b, x1, y1b, kShadingColor, pixels);
        }
    };

    auto renderDirection = [&](bool positiveSlope) {
        for (int32_t b = -maxLocalY; b <= maxLocalX; b += kShadingSpacing) {
            Point points[4];
            std::size_t count = 0;

            auto addPoint = [&](int32_t px, int32_t py) {
                if (px < 0 || px > maxLocalX || py < 0 || py > maxLocalY || count >= 4) {
                    return;
                }
                for (std::size_t i = 0; i < count; ++i) {
                    if (points[i].x == px && points[i].y == py) {
                        return;
                    }
                }
                points[count++] = {px, py};
            };

            if (positiveSlope) {
                addPoint(0, b);
                addPoint(maxLocalX, maxLocalX + b);
                addPoint(-b, 0);
                addPoint(maxLocalY - b, maxLocalY);
            } else {
                addPoint(0, b);
                addPoint(maxLocalX, -maxLocalX + b);
                addPoint(b, 0);
                addPoint(b - maxLocalY, maxLocalY);
            }

            if (count < 2) {
                continue;
            }

            const Point& first = points[0];
            std::size_t secondIndex = 1;
            while (secondIndex < count &&
                   points[secondIndex].x == first.x &&
                   points[secondIndex].y == first.y) {
                ++secondIndex;
            }
            if (secondIndex >= count) {
                continue;
            }

            const Point& second = points[secondIndex];
            const int32_t x0 = originX + first.x;
            const int32_t y0 = originY + first.y;
            const int32_t x1 = originX + second.x;
            const int32_t y1 = originY + second.y;
            const int32_t offsetSign = positiveSlope ? 1 : -1;
            drawThickLine(x0, y0, x1, y1, offsetSign);
        }
    };

    renderDirection(true);   // rising diagonal
    renderDirection(false);  // falling diagonal
}

void drawHorizontalFill(int32_t originX,
                        int32_t originY,
                        int32_t cellWidth,
                        int32_t cellHeight,
                        uint8_t* pixels,
                        int32_t thicknessPx) {
    if (!pixels || cellWidth <= 2 || cellHeight <= 2 || thicknessPx <= 0) {
        return;
    }

    const int32_t maxThickness = std::max<int32_t>(1, std::min<int32_t>(cellHeight - 2, kShadingSpacing));
    const int32_t thickness = std::max<int32_t>(1, std::min<int32_t>(thicknessPx, maxThickness));
    const int32_t maxY = originY + cellHeight - 1;
    for (int32_t y = originY; y <= maxY; y += kShadingSpacing) {
        for (int32_t t = 0; t < thickness; ++t) {
            const int32_t drawY = y + t;
            if (drawY > maxY) {
                break;
            }
            epd_draw_hline(originX, drawY, cellWidth, kShadingColor, pixels);
        }
    }
}

bool isValidMonth(int month) {
    return month >= 1 && month <= 12;
}

std::string twoDigitYear(int year) {
    char buffer[4];
    std::snprintf(buffer, sizeof(buffer), "%02d", ((year % 100) + 100) % 100);
    return buffer;
}

// Jeden měsíc -> "Rijen 2026", dva měsíce -> "Rij - Lis 2026", přes přelom roku -> "Pro 26 - Led 27".
std::string buildWindowTitle(const CalendarCell& first, const CalendarCell& last) {
    if (!isValidMonth(first.month) || !isValidMonth(last.month)) {
        return std::to_string(first.year);
    }

    if (first.year == last.year && first.month == last.month) {
        return std::string(kMonthNames[first.month - 1]) + " " + std::to_string(first.year);
    }

    if (first.year == last.year) {
        return std::string(kMonthAbbrev[first.month - 1]) + " - " + kMonthAbbrev[last.month - 1] + " " +
               std::to_string(first.year);
    }

    return std::string(kMonthAbbrev[first.month - 1]) + " " + twoDigitYear(first.year) + " - " +
           kMonthAbbrev[last.month - 1] + " " + twoDigitYear(last.year);
}

void drawCellHighlight(int32_t cellX, int32_t cellY, int32_t cellWidth, int32_t cellHeight, uint8_t* pixels) {
    constexpr int32_t kThickness = 3;
    for (int32_t t = 1; t <= kThickness; ++t) {
        const int32_t w = cellWidth - 2 * t;
        const int32_t h = cellHeight - 2 * t;
        if (w <= 0 || h <= 0) {
            break;
        }
        epd_draw_hline(cellX + t, cellY + t, w, 0, pixels);
        epd_draw_hline(cellX + t, cellY + cellHeight - t, w, 0, pixels);
        epd_draw_vline(cellX + t, cellY + t, h, 0, pixels);
        epd_draw_vline(cellX + cellWidth - t, cellY + t, h, 0, pixels);
    }
}

}  // namespace

void MonthFrame::handleMessage(const core::Message& message) {
    (void)message;
}

void MonthFrame::update(uint32_t elapsedMs) {
    (void)elapsedMs;
}

void MonthFrame::render(core::RenderContext& context) const {
    if (!context.pixels) {
        return;
    }

    const auto width = static_cast<int32_t>(context.display.width);
    const auto height = static_cast<int32_t>(context.display.height);
    const auto* font = reinterpret_cast<const GFXfont*>(context.font);
    if (!font) {
        font = &FiraSans;
    }

    epd_draw_hline(0, 0, width, 0, context.pixels);
    epd_draw_hline(0, height - 2, width, 0, context.pixels);
    epd_draw_vline(0, 0, height, 0, context.pixels);
    epd_draw_vline(width - 2, 0, height, 0, context.pixels);

    const int32_t separatorX = computeSeparatorX(width);
    epd_draw_vline(separatorX, 10, height - 20, 0, context.pixels);
    epd_draw_vline(separatorX + 1, 10, height - 20, 0, context.pixels);

    if (hasCalendarData_) {
        renderCalendar(context);
    }
}

void MonthFrame::setMonthLabel(std::string label) {
    monthLabel_ = std::move(label);
}

void MonthFrame::setCalendarData(const MonthCalendarData& data) {
    calendarData_ = data;
    hasCalendarData_ = data.valid;
}

void MonthFrame::renderCalendar(core::RenderContext& context) const {
    if (!context.pixels || !hasCalendarData_) {
        return;
    }

    const auto* font = reinterpret_cast<const GFXfont*>(context.font);
    if (!font) {
        font = &FiraSans;
    }

    const auto width = static_cast<int32_t>(context.display.width);
    const auto height = static_cast<int32_t>(context.display.height);
    const int32_t separatorX = computeSeparatorX(width);
    const int32_t leftPadding = 14;
    const int32_t rightPadding = 12;
    const int32_t calendarStartX = separatorX + leftPadding;
    const int32_t calendarWidth = std::max(0, width - calendarStartX - rightPadding);
    if (calendarWidth <= 0) {
        return;
    }

    const int32_t titleY = 28;
    const std::string monthTitle = buildWindowTitle(calendarData_.cells.front(), calendarData_.cells.back());
    drawTextLine(font, monthTitle, calendarStartX, titleY, context.pixels);

    const int32_t headerY = 63;
    const int32_t gridTop = headerY + 16;
    // pocet radek v kalendari
    const int32_t rows = kCalendarRows;
    const int32_t calendarHeight = std::max(0, height - gridTop - 18);
    const int32_t cellWidth = calendarWidth / 7;
    const int32_t cellHeight = rows > 0 ? calendarHeight / rows : 0;
    const int32_t gridHeight = cellHeight * rows;
    if (cellWidth <= 0 || cellHeight <= 0) {
        return;
    }

    for (int i = 0; i < 7; ++i) {
        int32_t labelX = calendarStartX + i * cellWidth + 4;
        drawTextLine(font, kWeekdayLabels[i], labelX, headerY, context.pixels);
    }

    for (int col = 0; col <= 7; ++col) {
        int32_t lineX = calendarStartX + col * cellWidth;
        epd_draw_vline(lineX, gridTop, gridHeight, 0, context.pixels);
    }

    for (int row = 0; row <= rows; ++row) {
        int32_t lineY = gridTop + row * cellHeight;
        epd_draw_hline(calendarStartX, lineY, cellWidth * 7, 0, context.pixels);
    }

    for (int i = 0; i < kCalendarCells; ++i) {
        const int col = i % 7;
        const int row = i / 7;
        const int32_t cellX = calendarStartX + col * cellWidth;
        const int32_t cellY = gridTop + row * cellHeight;
        const CalendarCell& cell = calendarData_.cells[i];

        // 1. den měsíce (a první buňka okna) nese i číslo měsíce, aby byl přechod měsíců zřetelný.
        char dayBuffer[8];
        if (cell.day == 1 || i == 0) {
            std::snprintf(dayBuffer, sizeof(dayBuffer), "%d.%d", cell.day, cell.month);
        } else {
            std::snprintf(dayBuffer, sizeof(dayBuffer), "%d", cell.day);
        }
        const auto count = calendarData_.eventCounts[i];
        const bool hasTimed = calendarData_.timedDayFlags[i] != 0;
        const uint8_t allDayCount = calendarData_.allDayCounts[i];
        const bool hasAllDay = allDayCount > 0;

        // Pokud existuje alespoň jedna časová událost, zachovat diagonální šrafování.
        if (hasTimed) {
            drawDiagonalShading(cellX + 1, cellY + 1, cellWidth - 2, cellHeight - 2, context.pixels);
        }

        // Pokud existuje alespoň jedna celodenní událost, doplnit přes to vodorovné čárky přes celé políčko.
        if (hasAllDay) {
            const int32_t thickness = 2 * static_cast<int32_t>(allDayCount);
            drawHorizontalFill(cellX + 1, cellY + 1, cellWidth - 2, cellHeight - 2, context.pixels, thickness);
        }

        if (measureTextWidth(font, dayBuffer) > cellWidth - 8) {
            drawTextHalf(font, dayBuffer, cellX + 4, cellY + 18, context.pixels);
        } else {
            drawTextLine(font, dayBuffer, cellX + 4, cellY + 18, context.pixels);
        }
        if (count > 0) {
            char countBuffer[6];
            std::snprintf(countBuffer, sizeof(countBuffer), "%u", static_cast<unsigned>(count));
            drawCenteredTextInCell(font, countBuffer, cellX, cellY, cellWidth, cellHeight, context.pixels);
        }

        if (i == calendarData_.todayIndex) {
            drawCellHighlight(cellX, cellY, cellWidth, cellHeight, context.pixels);
        }
    }
}

}  // namespace widgets
}  // namespace apps
