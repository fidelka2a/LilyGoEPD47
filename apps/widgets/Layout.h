#ifndef APPS_WIDGETS_LAYOUT_H
#define APPS_WIDGETS_LAYOUT_H

#include <cstdint>

namespace apps {
namespace widgets {

constexpr int32_t kSeparatorOffsetX = 40;

inline int32_t computeSeparatorX(int32_t displayWidth) {
    int32_t value = displayWidth / 2 + kSeparatorOffsetX;
    if (value < 0) {
        return 0;
    }
    if (value > displayWidth - 2) {
        return displayWidth - 2;
    }
    return value;
}

}  // namespace widgets
}  // namespace apps

#endif  // APPS_WIDGETS_LAYOUT_H
