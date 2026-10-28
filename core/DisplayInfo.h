#ifndef CORE_DISPLAY_INFO_H
#define CORE_DISPLAY_INFO_H

#include <cstdint>

namespace core {

enum class DisplayRotation : uint8_t {
    Landscape0 = 0,
    Landscape180,
    Portrait90,
    Portrait270
};

struct DisplayInfo {
    uint16_t width;
    uint16_t height;
    uint8_t colorDepth;
    DisplayRotation rotation;
    uint16_t refreshRateHz;

    constexpr DisplayInfo(uint16_t w = 960,
                          uint16_t h = 540,
                          uint8_t depth = 1,
                          DisplayRotation rot = DisplayRotation::Landscape0,
                          uint16_t refresh = 0)
        : width(w), height(h), colorDepth(depth), rotation(rot), refreshRateHz(refresh) {}
};

}  // namespace core

#endif  // CORE_DISPLAY_INFO_H
