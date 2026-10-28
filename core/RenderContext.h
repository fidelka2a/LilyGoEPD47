#ifndef CORE_RENDER_CONTEXT_H
#define CORE_RENDER_CONTEXT_H

#include <cstdint>

#include "DisplayInfo.h"
#include "Framebuffer.h"

namespace core {

struct ClipRect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = 0;
    int16_t height = 0;
};

struct RenderContext {
    DisplayInfo display;
    Framebuffer* framebuffer = nullptr;
    ClipRect clip;
    const void* font = nullptr;  // points to a font implementation understood by widgets
    uint8_t* pixels = nullptr;   // raw pointer obtained from framebuffer lock

    RenderContext() = default;
    RenderContext(const DisplayInfo& info, Framebuffer* fb) : display(info), framebuffer(fb) {
        clip.x = 0;
        clip.y = 0;
        clip.width = static_cast<int16_t>(info.width);
        clip.height = static_cast<int16_t>(info.height);
    }
};

}  // namespace core

#endif  // CORE_RENDER_CONTEXT_H
