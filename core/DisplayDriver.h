#ifndef CORE_DISPLAY_DRIVER_H
#define CORE_DISPLAY_DRIVER_H

#include "DisplayInfo.h"
#include "Framebuffer.h"

namespace core {

class DisplayDriver {
  public:
    virtual ~DisplayDriver() = default;

    virtual void begin(const DisplayInfo& info) = 0;
    virtual void present(Framebuffer& framebuffer) = 0;
};

}  // namespace core

#endif  // CORE_DISPLAY_DRIVER_H
