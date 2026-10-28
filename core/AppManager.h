#ifndef CORE_APP_MANAGER_H
#define CORE_APP_MANAGER_H

#include <cstdint>

#include "DisplayDriver.h"
#include "EventQueue.h"
#include "IApp.h"
#include "Services.h"

namespace core {

class AppManager {
  public:
    explicit AppManager(Services& services) : services_(services) {}

    void setActiveApp(IApp* app) {
        activeApp_ = app;
        if (activeApp_) {
            activeApp_->init(services_);
            needsRender_ = true;
        }
    }

    bool postMessage(const Message& message) {
        bool pushed = queue_.push(message);
        if (pushed) {
            needsRender_ = true;
        }
        return pushed;
    }

    void process(uint32_t elapsedMs, DisplayDriver& driver, RenderContext& context) {
        if (!activeApp_) {
            return;
        }

        if (!context.framebuffer) {
            return;
        }

        bool handledMessage = false;
        Message message;
        while (queue_.pop(message)) {
            activeApp_->handleMessage(message);
            handledMessage = true;
            needsRender_ = true;
        }

        if (!handledMessage && !needsRender_) {
            return;
        }

        activeApp_->update(elapsedMs);
        activeApp_->render(context);
        driver.present(*context.framebuffer);
        needsRender_ = false;
    }

  private:
    Services& services_;
    EventQueue queue_;
    IApp* activeApp_ = nullptr;
    bool needsRender_ = true;
};

}  // namespace core

#endif  // CORE_APP_MANAGER_H
