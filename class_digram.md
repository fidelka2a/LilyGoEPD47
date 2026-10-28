@startuml
title Calendar Display – Architecture (Core, Apps, Widgets)

skinparam packageStyle rect
skinparam classAttributeIconSize 0

package core {
  class DisplayInfo {
    +width : uint16_t
    +height : uint16_t
    +bpp : uint8_t
    +inverted : bool
    +rotation : Rotation
    --
    +DisplayInfo(w,h,bits,inv=false,rot=R0)
  }

  enum Rotation {
    R0
    R90
    R180
    R270
  }

  class Framebuffer {
    -_info : DisplayInfo&
    -_buf : std::vector<uint8_t>
    --
    +Framebuffer(info: DisplayInfo&)
    +data() : std::span<uint8_t>
    +data() : std::span<const uint8_t>
    +clear(value:uint8_t=0)
    +info() : const DisplayInfo&
    -calcSizeBytes(i:DisplayInfo) : size_t
  }

  class RenderContext {
    +info : const DisplayInfo&
    +fb : Framebuffer&
    --
    +drawPixel(x:int,y:int,on:bool)
    +fillRect(x:int,y:int,w:int,h:int,on:bool)
    +drawText(x:int,y:int,text:const char*,size:int=1)
  }

  ' --- Messaging
  class MsgTick {
    +dt : std::chrono::milliseconds
  }
  class MsgTime {
    +now : std::chrono::system_clock::time_point
  }
  class MsgNewEvents {
    +events : std::vector<Event>
  }
  class Event {
    +title : std::string
    +start : std::chrono::system_clock::time_point
    +allDay : bool
  }

  class Message <<union>>  {
    ..variant of..
    +MsgTick
    +MsgNewEvents
    +MsgTime
  }

  interface IApp {
    +init() : void
    +handle(m: Message) : void
    +update(dt: std::chrono::milliseconds) : void
    +render(ctx: RenderContext) : void
  }

  class AppManager {
    -_active : std::unique_ptr<IApp>
    --
    +setActive(app: std::unique_ptr<IApp>) : void
    +dispatch(m: Message) : void
    +update(dt: std::chrono::milliseconds) : void
    +render(ctx: RenderContext) : void
  }

  class DisplayDriver {
    -_info : const DisplayInfo&
    --
    +DisplayDriver(info: DisplayInfo&)
    +present(fb: Framebuffer) : void
  }
}

package apps {
  package widgets {
    class UpcomingEventsWidget {
      -_events : std::vector<core.Event>
      --
      +handle(m: core.Message) : void
      +render(ctx: core.RenderContext, x:int, y:int, w:int) : void
    }

    class MonthFrameWidget {
      -_now : std::chrono::system_clock::time_point
      --
      +handle(m: core.Message) : void
      +render(ctx: core.RenderContext, x:int, y:int, w:int, h:int) : void
    }
  }

  class CalendarApp {
    -_upcoming : widgets.UpcomingEventsWidget
    -_month : widgets.MonthFrameWidget
    --
    +init() : void
    +handle(m: core.Message) : void
    +update(dt: std::chrono::milliseconds) : void
    +render(ctx: core.RenderContext) : void
  }
}

' =======================
' Relationships (core)
' =======================
core.Framebuffer --> core.DisplayInfo : <<ref>>
core.RenderContext --> core.DisplayInfo : <<ref>>
core.RenderContext --> core.Framebuffer : <<ref>>
core.MsgNewEvents "1" o-- "0..*" core.Event
core.AppManager o-- core.IApp : «active»
core.DisplayDriver --> core.DisplayInfo : <<ref>>
core.DisplayDriver --> core.Framebuffer : present()

' Message union (visual link)
core.Message ..> core.MsgTick
core.Message ..> core.MsgNewEvents
core.Message ..> core.MsgTime

' =======================
' Relationships (apps)
' =======================
apps.CalendarApp ..|> core.IApp
apps.CalendarApp o-- apps.widgets.UpcomingEventsWidget
apps.CalendarApp o-- apps.widgets.MonthFrameWidget

' Widgets use rendering + messages
apps.widgets.UpcomingEventsWidget --> core.RenderContext : render()
apps.widgets.UpcomingEventsWidget --> core.Message : handle()
apps.widgets.MonthFrameWidget --> core.RenderContext : render()
apps.widgets.MonthFrameWidget --> core.Message : handle()

' App uses rendering + messages
apps.CalendarApp --> core.RenderContext : render()
apps.CalendarApp --> core.Message : handle()

' =======================
' Runtime wiring (main)
' =======================
core.AppManager --> apps.CalendarApp : setActive()
core.AppManager --> core.RenderContext : render(ctx)
core.AppManager --> core.Message : dispatch(m)

note top of core.RenderContext
  Sdílený přístup do framebufferu a metadat displeje.
  Zde lze řešit rotaci, clip, fonty a kreslící utility.
end note

note right of core.AppManager
  Řídí aktivní aplikaci:
  - předává zprávy (handle)
  - volá update(dt)
  - zajišťuje render(ctx)
end note

note bottom of apps.CalendarApp
  Kompozice UI: horní pás (MonthFrame),
  dole seznam 5 událostí (UpcomingEvents).
end note

@enduml
