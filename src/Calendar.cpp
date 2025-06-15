#include <iterator>
#include "Calendar.h"
#include <iostream>
#include <ArduinoJson.h>
#include "firasans.h"
#include <algorithm>

// Constructor
Calendar::Calendar(uint8_t* framebuffer) {
     this->framebuffer = framebuffer;
    this->eventWriter = new EventWriter(10,40,480,this->framebuffer);
    // Initialization if needed
}

// Destructor
Calendar::~Calendar() {
    Serial.println("Calendar destructor");
    delete this->eventWriter;
    // Cleanup if needed
}


void Calendar::draw(uint8_t* framebuffer)  {
      
}


// Method to add an event to the calendar
void Calendar::addEvent(const std::string& id,const std::string& name, const std::string& date, const std::string& time, const std::string& location) {
    events.emplace_back(id, name, date, time, location);
}

// Method to get the list of events
const std::vector<Calendar::Event>& Calendar::getEvents() const {
    return events;
}

// Method to display all events
void Calendar::displayEvents() const {
    std::cout<<"call displayEvents"<<std::endl;
    std::cout<<"address "<<&(this->framebuffer) <<std::endl;
    epd_poweron();

    // Smazani T5
    epd_clear();

    
    if (events.empty()) return;


    epd_draw_vline(220,10,200,0,this->framebuffer);
    epd_draw_vline(221,10,200,0,this->framebuffer);
    epd_draw_vline(222,10,200,0,this->framebuffer);
    epd_draw_vline(223,10,200,0,this->framebuffer);
    
    // epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, this->framebuffer);
    this->eventWriter->y = 40;
    delay(200);
    

    for (int event_id = 0; event_id < min(5,(int)events.size()); event_id++)
    {
        this->eventWriter->writeText(events[event_id].summary);
    }
    
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(500);
    //  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    //  epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, this->framebuffer);
    

    
    epd_poweroff();    
}

bool Calendar::parseJsonEvent(const JsonObject& obj, Event& event) {
    // Extract values from the JSON object
    event.id = obj["id"] | "";
    event.start = obj["start"] | "";
    event.end = obj["end"] | "";
    event.summary = obj["summary"] | "";
    event.location = obj["location"] | "";

    return !(event.id.empty() || event.start.empty() || event.end.empty()); // Return false if key data is missing
}


// Method to add events from a JSON array
// parse json 
// add new events to events object
// filter by id 
// order by date
// TODO: ted to jen plni do seznamu, je treba udelat filtr a seradit
bool Calendar::addEventsFromJsonArray(const std::string& jsonArray) {
    // Create a StaticJsonDocument large enough to hold the JSON array
    const size_t capacity = JSON_ARRAY_SIZE(10) + 10 * JSON_OBJECT_SIZE(5); // Adjust the size according to your needs
    StaticJsonDocument<capacity> doc;

    // Parse the JSON string
    DeserializationError error = deserializeJson(doc, jsonArray);
    if (error) {
        std::cerr << "Failed to parse JSON array: " << error.c_str() << std::endl;
        return false;
    }

    // Ensure that the parsed JSON is an array
    JsonArray array = doc.as<JsonArray>();
    if (array.isNull()) {
        std::cerr << "Invalid JSON format: not an array" << std::endl;
        return false;
    }

        std::cout << "pocet pred eventu # " << events.size() <<std::endl;
    // Iterate through each JSON object in the array and add events
    for (JsonObject obj : array) {
        Event event;
        if (parseJsonEvent(obj, event)) {
            events.push_back(event);
        } else {
            std::cerr << "Skipping event due to missing data" << std::endl;
        }
    }
    
    std::cout << "pocet  eventu # " << events.size() <<std::endl;

    // zapis udalosti na display
    this->displayEvents();

    return true;
}

// Method to format an event as a single string using sprintf
std::string Calendar::Event::toString() const {
    // Buffer large enough to hold the formatted string
    char buffer[300];

    // Use sprintf to create a formatted string with all event details
    sprintf(buffer, "%s ->  %s - %s kde: %s",
             summary.c_str(), start.c_str(), end.c_str(),  location.empty() ? "N/A" : location.c_str());

    // Return the formatted string as a std::string
    return std::string(buffer);
}

