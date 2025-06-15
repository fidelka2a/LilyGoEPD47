#ifndef CALENDAR_H
#define CALENDAR_H

#include <string>
#include <vector>
#include <ArduinoJson.h>
#include "EventWriter.h"
#include "Drawable.h"

class Calendar: public Drawable {
public:
    // Struct to hold calendar event details
    // lic id: string,
    //  public start: Date,
    //   public end:Date,
    //    public summary: string, 
    //    public location: string

    struct Event {
        std::string id;
        std::string summary;
        std::string start; // Ideally, a date type would be used instead of a string (e.g., std::chrono or other libraries)
        std::string end; // Same here for time
        std::string location;

      Event() = default;

      Event(const std::string& eventId, const std::string& eventSummary, const std::string& startDate, 
          const std::string& endDate, const std::string& eventLocation)
        : id(eventId), summary(eventSummary), start(startDate), end(endDate), location(eventLocation) {}

      std::string toString() const;
    };

    // Constructor and Destructor
    Calendar(uint8_t* framebuffer);
    Calendar() = default;
    ~Calendar();

    

    // Method to add a calendar event
    void addEvent(const std::string& id,const std::string& name, const std::string& date, const std::string& time, const std::string& location);

    void draw(uint8_t* framebuffer) override;

     
     bool addEventsFromJsonArray(const std::string& jsonArray);
     const std::vector<Event>& getEvents() const;
     void displayEvents() const;

private:
    std::vector<Event> events; // A collection of events
    EventWriter* eventWriter;
    uint8_t* framebuffer = nullptr;
    bool parseJsonEvent(const JsonObject& obj, Event& event);    
};

#endif // CALENDAR_H
