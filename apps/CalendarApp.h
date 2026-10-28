#ifndef APPS_CALENDAR_APP_H
#define APPS_CALENDAR_APP_H

#include <string>
#include <vector>

#include <ArduinoJson.h>

#include "core/IApp.h"
#include "core/Message.h"
#include "core/RenderContext.h"
#include "core/Services.h"
#include "widgets/MonthFrame.h"
#include "widgets/UpcomingEvents.h"

namespace apps {

struct CalendarEvent {
    std::string summary;
    std::string start;
    std::string end;
    std::string location;
};

class CalendarApp : public core::IApp {
  public:
    void init(core::Services& services) override;
    void handleMessage(const core::Message& message) override;
    void update(uint32_t elapsedMs) override;
    void render(core::RenderContext& context) override;

    void setEvents(std::vector<CalendarEvent> events);
    bool addEventsFromJsonArray(const std::string& jsonArray);
    const std::vector<CalendarEvent>& events() const { return events_; }

    // 255 = neznamy stav (default), nenakreslí indikátor
    void setBatteryPercent(uint8_t pct) { batteryPercent_ = pct; }
    void setSyncError(bool error) { syncError_ = error; }

  private:
    bool parseJsonEvent(const JsonObject& obj, CalendarEvent& event);

    std::vector<CalendarEvent> events_;
    core::Services* services_ = nullptr;
    widgets::UpcomingEvents upcomingEvents_;
    widgets::MonthFrame monthFrame_;
    uint8_t batteryPercent_ = 255;
    bool    syncError_      = false;
};

}  // namespace apps

#endif  // APPS_CALENDAR_APP_H
