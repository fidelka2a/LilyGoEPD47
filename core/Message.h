#ifndef CORE_MESSAGE_H
#define CORE_MESSAGE_H

#include <cstdint>
#include <string>

namespace core {

enum class MessageType : uint8_t {
    None = 0,
    Tick,
    ButtonPress,
    NetworkUpdate,
    Custom
};

struct Message {
    MessageType type = MessageType::None;
    uint32_t timestampMs = 0;
    std::string payload;
};

}  // namespace core

#endif  // CORE_MESSAGE_H
