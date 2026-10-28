#ifndef APPS_WIDGETS_UPCOMING_EVENTS_H
#define APPS_WIDGETS_UPCOMING_EVENTS_H

#include <vector>

#include "core/Message.h"
#include "core/RenderContext.h"

namespace apps {

struct CalendarEvent;

namespace widgets {

class UpcomingEvents {
  public:
    // Uloží snapshot událostí pro případ, že `render()` dostane prázdný seznam.
    void setEvents(const std::vector<CalendarEvent>& events);

    // Volitelný hook na zprávy (zatím nevyužitý); zachováno kvůli jednotnému API widgetů.
    void handleMessage(const core::Message& message);

    // Volitelný update hook (zatím nevyužitý).
    void update(uint32_t elapsedMs);

    // Vykreslí seznam událostí do levého panelu framebufferu.
    // Pokud je `events` prázdné, použije `cachedEvents_`.
    void render(core::RenderContext& context, const std::vector<CalendarEvent>& events) const;

  private:
    // Cache událostí ze `setEvents()`; použije se, když `render()` dostane prázdný seznam.
    std::vector<CalendarEvent> cachedEvents_;
};

}  // namespace widgets
}  // namespace apps

#endif  // APPS_WIDGETS_UPCOMING_EVENTS_H
