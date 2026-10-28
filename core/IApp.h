#ifndef CORE_IAPP_H
#define CORE_IAPP_H

#include "Message.h"
#include "RenderContext.h"

namespace core {

class Services;

class IApp {
  public:
    virtual ~IApp() = default;

    virtual void init(Services& services) = 0;
    virtual void handleMessage(const Message& message) = 0;
    virtual void update(uint32_t elapsedMs) = 0;
    virtual void render(RenderContext& context) = 0;
};

}  // namespace core

#endif  // CORE_IAPP_H
