# Copilot Instructions

These instructions help AI coding agents work productively in this PlatformIO project targeting the LilyGo T-Display S3 (ESP32-S3). Focus on the concrete project structure, build workflow, and patterns used across `src/` and `epd_driver/`.

## Big Picture
- **Platform/Board**: ESP32-S3 (LilyGo T-Display S3). Project is managed by PlatformIO via `platformio.ini`.
- **Main app code**: `src/` contains application logic:
  - `displayKitchen.cpp`: display-oriented routines for the T-Display; likely orchestrates drawing and UI updates.
  - `Calendar.{h,cpp}`: date/time utilities used by display and event features.
  - `EventWriter.{h,cpp}`: event logging/persistence functionality.
  - `Drawable.h`: abstraction for drawable entities on the display.
  - `pins.h`: board-specific pin mappings.
  - `arduino_secrets.h`: credentials/secrets (keep out of VCS when editing).
- **Display/E-Paper driver**: `epd_driver/` provides low-level display and data-bus code (I2S/RMT), font assets, and utilities. Even if the device is an LCD (T-Display), this folder contains drawing primitives and dependencies that the app builds on.
- **Third-party libraries**: `lib/ArduinoJson/` is vendored; used to parse/generate JSON for config or event serialization.
- **Build outputs**: `build/` holds compiled artifacts.

## Typical Data Flow
- UI flows start in `displayKitchen.cpp`, which uses `Drawable` patterns and driver functions in `epd_driver/` to render.
- Time/date is obtained or calculated via `Calendar.cpp`, which feeds formatted strings/values into display routines.
- Events/records are created and persisted through `EventWriter.cpp` (JSON-structured via ArduinoJson when needed).
- Hardware access goes through `pins.h` and low-level driver calls (I2S/RMT) in `epd_driver/`.

## Project Conventions
- **File organization**: High-level app logic in `src/`; hardware drivers and fonts in `epd_driver/`; external libs in `lib/`.
- **Headers**: Public interfaces in `*.h` in `src/` and `epd_driver/` with straightforward, minimal includes.
- **Drawing**: Use `Drawable`-like abstractions and helper routines in `epd_driver/` rather than direct register manipulation.
- **JSON**: Prefer `ArduinoJson` APIs from the vendored lib (see `lib/ArduinoJson/examples/*`).
- **Secrets**: Use `arduino_secrets.h` for tokens/SSIDs; avoid hardcoding credentials elsewhere.

## Build & Flash (PlatformIO)
Use PlatformIO tasks or these commands.

- **Build**:
  ```bash
  pio run
  ```
- **Upload (flash to device)**:
  ```bash
  pio run -t upload
  ```
- **Monitor (serial)**:
  ```bash
  pio run -t monitor
  ```
- **Clean**:
  ```bash
  pio run -t clean
  ```

Ensure the correct `env` is set in `platformio.ini` for the ESP32-S3 board. If multiple environments exist, target explicitly with `-e <envname>`.

## Debugging Tips
- **Serial logs**: Add `Serial.begin(115200);` early and `Serial.println()` calls in `displayKitchen.cpp`, `EventWriter.cpp`, and driver touchpoints.
- **Timing/date issues**: Check `Calendar.cpp` for formatting and epoch conversions.
- **Rendering glitches**: Confirm drawing sequence and buffer usage; review `epd_driver/` functions (`i2s_data_bus.*`, `rmt_pulse.*`, `font.c`).
- **Pin conflicts**: Verify `pins.h` aligns with the LilyGo T-Display S3 wiring. Update safely if hardware revision differs.

## Integration Points
- **ArduinoJson**: See examples in `lib/ArduinoJson/examples/` for idiomatic usage (e.g., `JsonParserExample`, `JsonGeneratorExample`). Use `StaticJsonDocument` or `DynamicJsonDocument` as appropriate for memory constraints on ESP32-S3.
- **Display driver**: Functions in `epd_driver/epd_driver.c` and helpers in `i2s_data_bus.c`, `rmt_pulse.c` handle low-level signaling. High-level app code should not bypass these helpers.

## Patterns & Examples
- **Drawable usage**: Implement new UI elements by creating structs/classes compatible with `Drawable.h` and render via driver utilities.
- **Event logging**: Use `EventWriter` to serialize events to JSON and persist or stream as needed; avoid ad-hoc file formats.
- **Calendar formatting**: Centralize date/time strings via `Calendar.h` APIs to ensure consistent display.

## When Adding Code
- Prefer placing new display logic in `displayKitchen.cpp` or adjacent modules rather than modifying low-level `epd_driver/` unless necessary.
- Add new hardware features behind clear interfaces in `src/` and call into existing bus helpers.
- Keep memory usage in check; avoid large dynamic allocations on the ESP32-S3.
- Update `pins.h` only with verified changes; document any hardware assumptions in comments.

## Key Files
- `platformio.ini`: board/env configuration.
- `src/displayKitchen.cpp`: main display orchestration.
- `src/Calendar.{h,cpp}`: time/date utilities.
- `src/EventWriter.{h,cpp}`: event serialization/persistence.
- `src/Drawable.h`: UI abstraction contract.
- `src/pins.h`: hardware pin mapping.
- `epd_driver/*`: low-level display and bus implementations.
- `lib/ArduinoJson/*`: JSON library and examples.

---
If any of these sections feel incomplete or inaccurate (e.g., different board/env in `platformio.ini`, or display driver specifics), please share details and I’ll refine the instructions accordingly.
