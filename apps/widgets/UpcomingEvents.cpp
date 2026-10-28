#include "UpcomingEvents.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>

#include <Arduino.h>
#include <epd_driver.h>

#include "apps/CalendarApp.h"
#include "core/RenderContext.h"
#include "firasans.h"
#include "Layout.h"
#include "ScaledText.h"

namespace apps {
namespace widgets {

namespace {

// Vrátí počet bajtů UTF-8 sekvence podle prvního bajtu.
static std::size_t utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xC0) return 1; // stray continuation byte – ošetřit jako 1
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

// Maximální počet událostí vykreslených v levém seznamu.
constexpr std::size_t kMaxEvents = 4;
// Levé odsazení textu v levém panelu.
constexpr int32_t kStartX = 20;
// Výchozí baseline Y pro první řádek výpisu.
constexpr int32_t kInitialY = 40;
// Svislý krok po vykreslení jednoho textového řádku.
constexpr int32_t kLineHeight = 32;
// Odsazení vodorovné oddělovací čáry od levého okraje a od svislé dělící čáry.
constexpr int32_t kDividerMarginX = 5;
// Posun Y oddělovací čáry vůči konci posledního řádku události.
constexpr int32_t kDividerGapBelowEventY = -21;
// Dodatečná mezera mezi čárou a baseline prvního řádku další události.
constexpr int32_t kDividerGapAboveNextTextY = 3;

// Vykreslí text na více řádků se základním zalamováním podle šířky.
// Pokud je `halfScale` true, použije poloviční měřítko (pro menší popisky / location).
void drawWrappedText(const std::string& text,
                     int32_t startX,
                     int32_t& cursorY,
                     int32_t maxWidth,
                     bool halfScale,
                     const GFXfont* font,
                     uint8_t* pixels) {
    if (!font || !pixels || text.empty()) {
        return;
    }

    // Jeden string znovupouzivany pro mereni i kresleni – zadne heap alokace uvnitr smycky.
    std::string candidate;
    candidate.reserve(text.size() + 1);

    std::size_t offset = 0;
    while (offset < text.size()) {
        std::size_t bestBreak = offset;
        std::size_t probe = offset;
        while (probe < text.size()) {
            probe += utf8CharLen(static_cast<unsigned char>(text[probe]));
            if (probe > text.size()) probe = text.size();
            candidate.assign(text, offset, probe - offset);
            const int32_t width = halfScale ? measureTextWidthHalf(font, candidate) : measureTextWidth(font, candidate);
            if (maxWidth > 0 && width > maxWidth) {
                if (bestBreak == offset) {
                    bestBreak = probe - utf8CharLen(static_cast<unsigned char>(text[offset]));
                    if (bestBreak <= offset) bestBreak = probe;
                }
                break;
            }
            bestBreak = probe;
            char lastChar = candidate.empty() ? '\0' : candidate.back();
            if (std::isspace(static_cast<unsigned char>(lastChar))) {
                bestBreak = probe;
            }

            if (probe == text.size()) {
                break;
            }
        }

        if (bestBreak <= offset) {
            bestBreak = std::min(text.size(), offset + utf8CharLen(static_cast<unsigned char>(text[offset])));
        }

        candidate.assign(text, offset, bestBreak - offset);
        int32_t x = startX;
        int32_t y = cursorY;
        if (halfScale) {
            drawTextHalf(font, candidate, x, y, pixels);
        } else {
            writeln((GFXfont*)font, candidate.c_str(), &x, &y, pixels);
        }
        cursorY += kLineHeight;
        offset = bestBreak;
        while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) {
            ++offset;
        }
    }
}

// Vrátí true, pokud řetězec vypadá jako ISO-8601 datetime: "YYYY-MM-DDTHH:MM...".
bool hasTimeComponent(const std::string& isoDateTime) {
    return isoDateTime.size() >= 16 && isoDateTime[10] == 'T';
}

struct DateInfo {
    int year = 0;
    int month = 0;
    int day = 0;
    bool valid = false;
};

bool isLeapYear(int year) {
    if (year % 400 == 0) {
        return true;
    }
    if (year % 100 == 0) {
        return false;
    }
    return (year % 4) == 0;
}

int daysInMonth(int year, int month) {
    static constexpr int kMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 31;
    }
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kMonthDays[month - 1];
}

bool parseIsoDate(const std::string& value, DateInfo& out) {
    if (value.size() < 10) {
        return false;
    }

    for (int idx : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!std::isdigit(static_cast<unsigned char>(value[idx]))) {
            return false;
        }
    }

    if (value[4] != '-' || value[7] != '-') {
        return false;
    }

    out.year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 + (value[2] - '0') * 10 + (value[3] - '0');
    out.month = (value[5] - '0') * 10 + (value[6] - '0');
    out.day = (value[8] - '0') * 10 + (value[9] - '0');

    const int dim = daysInMonth(out.year, out.month);
    if (out.month < 1 || out.month > 12 || out.day < 1 || out.day > dim) {
        return false;
    }

    out.valid = true;
    return true;
}

bool isSameDay(const DateInfo& lhs, const DateInfo& rhs) {
    return lhs.valid && rhs.valid && lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day;
}

void incrementDay(DateInfo& date) {
    if (!date.valid) {
        return;
    }

    const int dim = daysInMonth(date.year, date.month);
    if (date.day < dim) {
        ++date.day;
        return;
    }

    date.day = 1;
    if (date.month < 12) {
        ++date.month;
        return;
    }

    date.month = 1;
    ++date.year;
}

bool isNextDay(const DateInfo& start, const DateInfo& maybeNext) {
    if (!start.valid || !maybeNext.valid) {
        return false;
    }
    DateInfo tmp = start;
    incrementDay(tmp);
    return isSameDay(tmp, maybeNext);
}

DateInfo getCurrentDate() {
    DateInfo current;
    time_t now = 0;
    time(&now);
    if (now <= 0) {
        return current;
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    current.year = timeinfo.tm_year + 1900;
    current.month = timeinfo.tm_mon + 1;
    current.day = timeinfo.tm_mday;
    current.valid = true;
    return current;
}

// Převede ISO datum "YYYY-MM-DD" (nebo datetime) na formát "DD.MM".
std::string formatDottedDate(const std::string& isoDateTime) {
    if (isoDateTime.size() < 10) {
        return isoDateTime;
    }

    // Expected: YYYY-MM-DD...
    if (isoDateTime[4] != '-' || isoDateTime[7] != '-') {
        return isoDateTime.substr(0, 10);
    }

    const std::string mm = isoDateTime.substr(5, 2);
    const std::string dd = isoDateTime.substr(8, 2);
    return dd + "." + mm;
}

// Vytáhne čas "HH:MM" z ISO datetime. Pro celodenní datum vrací prázdný řetězec.
std::string formatTimeHHMM(const std::string& isoDateTime) {
    if (isoDateTime.size() >= 16 && isoDateTime[10] == 'T') {
        return isoDateTime.substr(11, 5);
    }
    return {};
}

// Naformátuje ISO datum/datetime na "DD.MM" nebo "DD.MM HH:MM".
std::string formatDottedDateTime(const std::string& isoDateTime) {
    const std::string date = formatDottedDate(isoDateTime);
    const std::string time = formatTimeHHMM(isoDateTime);
    if (!time.empty() && date.size() >= 5) {
        return date + " " + time;
    }
    return date;
}

// Vykreslí řádek "label: hodnota", kde label je menší (half-scale)
// a hodnota je normálním fontem se zalamováním.
void drawLabeledValueLine(const std::string& label,
                          const std::string& value,
                          int32_t startX,
                          int32_t& cursorY,
                          int32_t maxWidth,
                          const GFXfont* font,
                          uint8_t* pixels) {
    if (!font || !pixels) {
        return;
    }

    const int32_t labelWidth = measureTextWidthHalf(font, label);
    const int32_t valueX = startX + labelWidth + 4;
    const int32_t valueWidth = std::max<int32_t>(0, maxWidth - (valueX - startX));
    drawTextHalf(font, label, startX, cursorY, pixels);
    drawWrappedText(value, valueX, cursorY, valueWidth, false, font, pixels);
}

}  // namespace

void UpcomingEvents::setEvents(const std::vector<CalendarEvent>& events) {
    // Cache kopii, aby widget mohl vykreslit i když `render()` dostane prázdný seznam.
    cachedEvents_ = events;
}

void UpcomingEvents::handleMessage(const core::Message& message) {
    // Zatím nevyužito; zachováno pro jednotné API widgetů.
    (void)message;
}

void UpcomingEvents::update(uint32_t elapsedMs) {
    // Zatím nevyužito; události se aktualizují přes `setEvents()`.
    (void)elapsedMs;
}

void UpcomingEvents::render(core::RenderContext& context, const std::vector<CalendarEvent>& events) const {
    if (!context.pixels) {
        return;
    }

    const DateInfo today = getCurrentDate();

    const auto* font = reinterpret_cast<const GFXfont*>(context.font);
    if (!font) {
        font = &FiraSans;
    }

    // Preferuje explicitně předané události; jinak použije cache.
    const auto& source = events.empty() ? cachedEvents_ : events;
    const std::size_t count = std::min<std::size_t>(source.size(), kMaxEvents);

    int32_t cursorY = kInitialY;
    const auto width = static_cast<int32_t>(context.display.width);
    const int32_t separatorX = computeSeparatorX(width);
    // Mezera k svislé dělící čáře, aby text nepřetékal do kalendáře.
    const int32_t maxTextWidth = std::max<int32_t>(0, separatorX - kStartX - 12);

    for (std::size_t i = 0; i < count; ++i) {
        const auto& event = source[i];

        const bool startHasTime = hasTimeComponent(event.start);
        const bool endHasTime = hasTimeComponent(event.end);
        DateInfo startDate;
        DateInfo endDate;
        const bool startDateValid = parseIsoDate(event.start, startDate);
        const bool endDateValid = parseIsoDate(event.end, endDate);

        // Pro all-day eventy bývá "end" end-exclusive (např. 2025-01-01 .. 2025-01-02 znamená jen 1.1.).
        const bool sameDateLiteral = event.start.size() >= 10 && event.end.size() >= 10 && event.start.substr(0, 10) == event.end.substr(0, 10);
        const bool singleAllDayEndExclusive = !startHasTime && !endHasTime && startDateValid && endDateValid && isNextDay(startDate, endDate);
        const bool sameDay = sameDateLiteral || singleAllDayEndExclusive;

        const bool isTodaySingleDay = today.valid && startDateValid && sameDay && isSameDay(startDate, today);
        const std::string dayLabel = isTodaySingleDay ? "dnes" : formatDottedDate(event.start);

        // Vždy nejdřív summary.
        drawWrappedText(event.summary, kStartX, cursorY, maxTextWidth, false, font, context.pixels);

        // Pravidla výpisu data/času:
        // 1) celodenní stejný den (bez času) -> "DD.MM"
        // 2) časová událost ve stejný den -> "DD.MM HH:MM - HH:MM"
        // 3) jinak -> "od:" / "do:" ve formátu "DD.MM" nebo "DD.MM HH:MM"
        if (!startHasTime && !endHasTime && sameDay) {
            drawWrappedText(dayLabel, kStartX, cursorY, maxTextWidth, false, font, context.pixels);
        } else if (startHasTime && endHasTime && sameDay) {
            const std::string startTime = formatTimeHHMM(event.start);
            const std::string endTime = formatTimeHHMM(event.end);
            const std::string line = dayLabel + " " + startTime + " - " + endTime;
            drawWrappedText(line, kStartX, cursorY, maxTextWidth, false, font, context.pixels);
        } else {
            const std::string startFormatted = formatDottedDateTime(event.start);
            const std::string endFormatted = formatDottedDateTime(event.end);

            if (!event.start.empty() && !event.end.empty() && (!startHasTime && !endHasTime)) {
                const int32_t baselineY = cursorY;
                int32_t x = kStartX;
                drawTextHalf(font, "od:", x, baselineY, context.pixels);
                x += measureTextWidthHalf(font, "od:") + 4;
                int32_t y = baselineY;
                writeln((GFXfont*)font, startFormatted.c_str(), &x, &y, context.pixels);

                if (endFormatted != startFormatted) {
                    x += 10;
                    drawTextHalf(font, "do:", x, baselineY, context.pixels);
                    x += measureTextWidthHalf(font, "do:") + 4;
                    y = baselineY;
                    writeln((GFXfont*)font, endFormatted.c_str(), &x, &y, context.pixels);
                }
                cursorY += kLineHeight;
            } else {
                if (!event.start.empty()) {
                    drawLabeledValueLine("od:", startFormatted, kStartX, cursorY, maxTextWidth, font, context.pixels);
                }
                if (!event.end.empty()) {
                    drawLabeledValueLine("do:", endFormatted, kStartX, cursorY, maxTextWidth, font, context.pixels);
                }
            }
        }

        // Volitelný řádek s location (menším písmem kvůli úspoře místa).
        if (!event.location.empty()) {
            std::string locationStr = std::string("kde: ") + event.location;
            drawWrappedText(locationStr, kStartX, cursorY, maxTextWidth, true, font, context.pixels);
        }

        // Oddělovací čára mezi událostmi. Nechá padding od levého okraje i od svislé dělící čáry.
        const int32_t dividerStartX = kDividerMarginX;
        const int32_t dividerEndX = separatorX - kDividerMarginX;
        const int32_t dividerWidth = std::max<int32_t>(0, dividerEndX - dividerStartX);
        const int32_t dividerY = cursorY + kDividerGapBelowEventY;
        if (dividerWidth > 0) {
            epd_draw_hline(dividerStartX, dividerY, dividerWidth, 0, context.pixels);
        }

        // Další událost začne pod čárou s bezpečnou mezerou podle baseline fontu.
        cursorY = dividerY + font->ascender + kDividerGapAboveNextTextY;
    }
}

}  // namespace widgets
}  // namespace apps
