#ifndef APPS_WIDGETS_MONTH_FRAME_H
#define APPS_WIDGETS_MONTH_FRAME_H

#include <array>
#include <cstdint>
#include <string>

#include "core/Message.h"
#include "core/RenderContext.h"

namespace apps {
namespace widgets {

struct CalendarCell {
    int year = 1970;
    int month = 1;
    int day = 1;
};

constexpr int kCalendarRows = 5;
constexpr int kCalendarCells = kCalendarRows * 7;

// Klouzavý kalendář: index 0 == pondělí týdne, ve kterém je dnešek, pak souvisle dalších 34 dní.
struct MonthCalendarData {
    std::array<CalendarCell, kCalendarCells> cells{};
    int todayIndex = -1;
    std::array<uint8_t, kCalendarCells> eventCounts{};
    // Buňky, ve kterých existuje alespoň jedna časová událost (pro diagonální šrafování).
    std::array<uint8_t, kCalendarCells> timedDayFlags{};
    // Počet "celodenních" událostí v dané buňce (počítá i vícedenní události) pro vodorovné šrafování.
    std::array<uint8_t, kCalendarCells> allDayCounts{};
    bool valid = false;
};

class MonthFrame {
  public:
    void handleMessage(const core::Message& message);
    void update(uint32_t elapsedMs);
    void render(core::RenderContext& context) const;

    void setMonthLabel(std::string label);
    void setCalendarData(const MonthCalendarData& data);

  private:
    void renderCalendar(core::RenderContext& context) const;

    std::string monthLabel_;
    MonthCalendarData calendarData_;
    bool hasCalendarData_ = false;
};

}  // namespace widgets
}  // namespace apps

#endif  // APPS_WIDGETS_MONTH_FRAME_H
