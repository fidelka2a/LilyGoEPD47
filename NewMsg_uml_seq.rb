@startuml
participant "loop()" as Loop
participant "mqttClient" as MQTT
participant "onMqttMessage()" as Callback
participant "core::AppManager" as AppManager
participant "core::EventQueue" as Queue
participant "apps::CalendarApp" as CalendarApp
participant "widgets::MonthFrame" as MonthFrame
participant "widgets::UpcomingEvents" as Upcoming

Loop -> MQTT: poll()
MQTT --> Callback: onMessage(messageSize)
Callback -> Callback: přečti payload\nmqttClient.read()
Callback -> Callback: sestav core::Message{NetworkUpdate}
Callback -> AppManager: postMessage(message)
AppManager -> Queue: push(message)

Loop -> AppManager: process(elapsed,…)
AppManager -> Queue: pop(message)
AppManager -> CalendarApp: handleMessage(message)

CalendarApp -> MonthFrame: handleMessage(message)
CalendarApp -> Upcoming: handleMessage(message)

CalendarApp -> CalendarApp: addEventsFromJsonArray(payload)
loop přes události
    CalendarApp -> CalendarApp: parseJsonEvent(obj)
end
CalendarApp -> CalendarApp: seřaď události
CalendarApp -> Upcoming: setEvents(events)
@enduml
