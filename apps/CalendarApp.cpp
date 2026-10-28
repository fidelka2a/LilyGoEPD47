#include "CalendarApp.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>
#include <utility>

#include <Arduino.h>

#include "Utf8.h"
#include "widgets/ScaledText.h"  // transitively includes epd_driver.h

namespace apps {

namespace {
constexpr uint8_t kClearColor = 0xFF;

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

int weekdayMondayZero(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        --year;
    }

    const int K = year % 100;
    const int J = year / 100;
    const int h = (day + (13 * (month + 1)) / 5 + K + (K / 4) + (J / 4) + 5 * J) % 7;
    const int sundayZero = (h + 6) % 7;
    return (sundayZero + 6) % 7;  // Shift so Monday == 0
}

DateInfo getCurrentDate() {
    DateInfo current;
    const std::time_t now = std::time(nullptr);
    if (now <= 0) {
        return current;
    }

    std::tm* info = std::localtime(&now);
    if (!info) {
        return current;
    }

    current.year = info->tm_year + 1900;
    current.month = info->tm_mon + 1;
    current.day = info->tm_mday;
    current.valid = true;
    return current;
}

bool isEarlier(const DateInfo& lhs, const DateInfo& rhs) {
    if (lhs.year != rhs.year) {
        return lhs.year < rhs.year;
    }
    if (lhs.month != rhs.month) {
        return lhs.month < rhs.month;
    }
    return lhs.day < rhs.day;
}

bool isSameDay(const DateInfo& lhs, const DateInfo& rhs) {
    return lhs.valid && rhs.valid && lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day;
}

bool hasIsoTimeComponent(const std::string& value) {
    // "YYYY-MM-DD" = all-day, "YYYY-MM-DDTHH:MM:SS" = timed.
    return value.size() >= 11 && value[10] == 'T';
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

void decrementDay(DateInfo& date) {
    if (!date.valid) {
        return;
    }

    if (date.day > 1) {
        --date.day;
        return;
    }

    if (date.month > 1) {
        --date.month;
    } else {
        date.month = 12;
        --date.year;
    }
    date.day = daysInMonth(date.year, date.month);
}

DateInfo earliestEventDate(const std::vector<CalendarEvent>& events) {
    DateInfo best;
    for (const auto& evt : events) {
        DateInfo parsed;
        if (!parseIsoDate(evt.start, parsed)) {
            continue;
        }

        if (!best.valid || isEarlier(parsed, best)) {
            best = parsed;
        }
    }
    return best;
}

widgets::MonthCalendarData buildCalendarData(const std::vector<CalendarEvent>& events) {
    widgets::MonthCalendarData data;

    DateInfo reference = getCurrentDate();
    if (!reference.valid) {
        reference = earliestEventDate(events);
    }

    if (!reference.valid) {
        return data;
    }

    // Klouzavé okno začíná pondělím aktuálního týdne.
    const int weekday = weekdayMondayZero(reference.year, reference.month, reference.day);
    DateInfo cursorDay = reference;
    for (int i = 0; i < weekday; ++i) {
        decrementDay(cursorDay);
    }

    data.valid = true;
    data.todayIndex = weekday;
    for (auto& cell : data.cells) {
        cell.year = cursorDay.year;
        cell.month = cursorDay.month;
        cell.day = cursorDay.day;
        incrementDay(cursorDay);
    }
    data.eventCounts.fill(0);
    data.timedDayFlags.fill(0);
    data.allDayCounts.fill(0);

    auto cellIndexOf = [&](const DateInfo& day) -> int {
        for (int i = 0; i < widgets::kCalendarCells; ++i) {
            const auto& cell = data.cells[i];
            if (cell.year == day.year && cell.month == day.month && cell.day == day.day) {
                return i;
            }
        }
        return -1;
    };

    for (const auto& evt : events) {
        DateInfo startDate;
        DateInfo endDate;
        if (!parseIsoDate(evt.start, startDate) || !parseIsoDate(evt.end, endDate)) {
            continue;
        }

        DateInfo rangeStart = startDate;
        DateInfo rangeEnd = endDate;
        if (isEarlier(rangeEnd, rangeStart)) {
            std::swap(rangeStart, rangeEnd);
        }

        const bool isSame = isSameDay(rangeStart, rangeEnd);
        const bool hasTime = hasIsoTimeComponent(evt.start) || hasIsoTimeComponent(evt.end);
        const bool isTimedSameDay = hasTime && isSame;
        // Pro kalendář: každá vícedenní událost se bere jako "celodenní" (vodorovné čárky),
        // diagonální šrafování je jen pro časové události v rámci jednoho dne.
        const bool isAllDayLike = !hasTime || !isSame;

        // Číselný počet: pro celodenní vícedenní událost pouze v 1. dni akce, jinak ve všech zasažených dnech.
        // Rozsah "end" je pro vícedenné události end-exclusive (např. 24..26 => 24 a 25).

        auto markDay = [&](const DateInfo& day) {
            const int index = cellIndexOf(day);
            if (index < 0) {
                return;
            }

            if (isAllDayLike) {
                auto& counter = data.allDayCounts[index];
                if (counter < std::numeric_limits<uint8_t>::max()) {
                    counter++;
                }
            } else if (isTimedSameDay) {
                data.timedDayFlags[index] = 1;
            }
        };

        auto incrementCount = [&](const DateInfo& day) {
            const int index = cellIndexOf(day);
            if (index < 0) {
                return;
            }
            auto& counter = data.eventCounts[index];
            if (counter < std::numeric_limits<uint8_t>::max()) {
                counter++;
            }
        };

        if (isSame) {
            markDay(rangeStart);
            incrementCount(rangeStart);
            continue;
        }

        // Vícedenná událost: rozsah je [start, end).
        DateInfo cursor = rangeStart;
        int safety = 0;
        while (cursor.valid && isEarlier(cursor, rangeEnd) && safety < 370) {
            markDay(cursor);

            incrementDay(cursor);
            ++safety;
        }

        // Vícedenná (all-day i časová) se v kalendáři chová jako celodenní: číslice pouze v 1. dni akce.
        incrementCount(rangeStart);
    }

    return data;
}

}  // namespace

namespace {

// Ikona baterie + procenta v pravem hornim rohu displeje (960x540).
// Pouziva jen epd_draw_hline / epd_draw_vline / drawTextHalf – bez epd_fill_rect.
void drawBatteryIndicator(uint8_t pct, const GFXfont* font, uint8_t* pixels) {
    if (!pixels || !font) return;

    constexpr int32_t kBodyW  = 32;
    constexpr int32_t kBodyH  = 14;
    constexpr int32_t kNubW   = 4;
    constexpr int32_t kNubH   = 8;
    constexpr int32_t kMargin = 8;
    constexpr int32_t kTop    = 10;
    constexpr uint8_t kBlack  = 0x00;

    const int32_t bx = EPD_WIDTH - kMargin - kNubW - kBodyW;
    const int32_t by = kTop;

    // Obrys tela baterie (4 strany pomoci hline/vline)
    epd_draw_hline(bx,           by,           kBodyW, kBlack, pixels);
    epd_draw_hline(bx,           by + kBodyH,  kBodyW, kBlack, pixels);
    epd_draw_vline(bx,           by,           kBodyH, kBlack, pixels);
    epd_draw_vline(bx + kBodyW,  by,           kBodyH, kBlack, pixels);

    // Kontaktni nub (kladny pol) – vyplneny pruhy hline
    const int32_t nubY = by + (kBodyH - kNubH) / 2;
    for (int32_t fy = nubY; fy < nubY + kNubH; ++fy) {
        epd_draw_hline(bx + kBodyW + 1, fy, kNubW, kBlack, pixels);
    }

    // Vyplneni proporcionalni k nabiti (s 2px paddingem)
    const int32_t maxFill = kBodyW - 4;
    const int32_t fillW   = (maxFill * pct) / 100;
    for (int32_t fy = by + 2; fy < by + kBodyH - 1; ++fy) {
        if (fillW > 0) {
            epd_draw_hline(bx + 2, fy, fillW, kBlack, pixels);
        }
    }

    // Procentualni text nalevo od ikony
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    const int32_t textW = widgets::measureTextWidthHalf(font, buf);
    const int32_t tx    = bx - textW - 6;
    const int32_t ty    = by + kBodyH - 1;
    widgets::drawTextHalf(font, buf, tx, ty, pixels);
}

// Symbol chyby synchronizace: vykricnik (3px siroky) vlevo od indikátoru baterie.
// Vykresluje se ve stejne vysce jako baterie, takze nezasahuje do kalendare pod nim.
void drawSyncErrorSymbol(const GFXfont* font, uint8_t* pixels) {
    if (!pixels || !font) return;

    constexpr int32_t kBodyW  = 32;
    constexpr int32_t kNubW   = 4;
    constexpr int32_t kBodyH  = 14;
    constexpr int32_t kMargin = 8;
    constexpr int32_t kTop    = 10;
    constexpr uint8_t kBlack  = 0x00;

    // Levy okraj baterie (stejny vypocet jako v drawBatteryIndicator)
    const int32_t bx = EPD_WIDTH - kMargin - kNubW - kBodyW;
    const int32_t by = kTop;

    // Odhadneme sirku textu procent (nejhorsi pripad "100%")
    char worstCase[] = "100%";
    const int32_t battTextW = widgets::measureTextWidthHalf(font, worstCase);

    // Symbol vlevo od textu procent s malou mezerou
    constexpr int32_t kSymW   = 3;   // sirka tela vykricniku
    constexpr int32_t kGap    = 8;   // mezera vlevo od textu procent
    const int32_t sx = bx - battTextW - kGap - kSymW - 4;

    // Telo vykricniku: svisle cary od ry=by+1 do ry=by+kBodyH-4
    const int32_t bodyTop    = by + 1;
    const int32_t bodyBottom = by + kBodyH - 4;
    for (int32_t ry = bodyTop; ry <= bodyBottom; ++ry) {
        epd_draw_hline(sx, ry, kSymW, kBlack, pixels);
    }
    // Tecka: 2 px pod teletem
    const int32_t dotY = bodyBottom + 2;
    epd_draw_hline(sx, dotY,     kSymW, kBlack, pixels);
    epd_draw_hline(sx, dotY + 1, kSymW, kBlack, pixels);
}

}  // namespace

void CalendarApp::init(core::Services& services) {
    services_ = &services;
    monthFrame_.setMonthLabel("Upcoming");
    upcomingEvents_.setEvents(events_);
}

void CalendarApp::handleMessage(const core::Message& message) {
    monthFrame_.handleMessage(message);
    upcomingEvents_.handleMessage(message);

    if (message.type == core::MessageType::NetworkUpdate) {
        if (addEventsFromJsonArray(message.payload)) {
            Serial.println("[CalendarApp] Events updated from message payload.");
        } else {
            Serial.println("[CalendarApp] Failed to parse event payload.");
        }
    }
}

void CalendarApp::update(uint32_t elapsedMs) {
    monthFrame_.update(elapsedMs);
    upcomingEvents_.update(elapsedMs);
}

void CalendarApp::render(core::RenderContext& context) {
    if (!context.framebuffer) {
        return;
    }

    auto span = context.framebuffer->acquireSpan();
    context.pixels = span.data();

    if (span.size() > 0) {
        std::memset(context.pixels, kClearColor, span.size());
    }

    if (services_ && services_->fonts) {
        context.font = services_->fonts->getDefaultFont();
    }

    monthFrame_.render(context);
    upcomingEvents_.render(context, events_);

    if (batteryPercent_ <= 100) {
        drawBatteryIndicator(batteryPercent_,
                             static_cast<const GFXfont*>(context.font),
                             context.pixels);
    }

    if (syncError_) {
        drawSyncErrorSymbol(static_cast<const GFXfont*>(context.font),
                            context.pixels);
    }

    context.framebuffer->releaseSpan();
    context.pixels = nullptr;
}

void CalendarApp::setEvents(std::vector<CalendarEvent> events) {
    events_ = std::move(events);
    upcomingEvents_.setEvents(events_);
    monthFrame_.setCalendarData(buildCalendarData(events_));
}

bool CalendarApp::addEventsFromJsonArray(const std::string& jsonArray) {
    if (jsonArray.empty()) {
        return false;
    }

    DynamicJsonDocument doc(8192);
    auto error = deserializeJson(doc, jsonArray);
    if (error) {
        Serial.print("[CalendarApp] JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonArray array = doc.as<JsonArray>();
    if (array.isNull()) {
        return false;
    }

    std::vector<CalendarEvent> parsed;
    parsed.reserve(array.size());

    for (JsonObject obj : array) {
        CalendarEvent evt;
        if (parseJsonEvent(obj, evt)) {
            parsed.push_back(std::move(evt));
        } else {
            Serial.println("[CalendarApp] Skipping malformed event.");
        }
    }

    std::sort(parsed.begin(), parsed.end(), [](const CalendarEvent& a, const CalendarEvent& b) {
        return a.start < b.start;
    });

    setEvents(std::move(parsed));

    return !events_.empty();
}

bool CalendarApp::parseJsonEvent(const JsonObject& obj, CalendarEvent& event) {
    event.summary = ensureSafeUtf8ForFont(obj["summary"] | "");
    event.start = obj["start"] | "";
    event.end = obj["end"] | "";
    event.location = ensureSafeUtf8ForFont(obj["location"] | "");

    return !(event.summary.empty() || event.start.empty() || event.end.empty());
}

}  // namespace apps
