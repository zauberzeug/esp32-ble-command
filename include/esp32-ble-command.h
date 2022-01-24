#ifndef ESP32LIZARDBLE_H
#define ESP32LIZARDBLE_H

#include <functional>
#include <string_view>

namespace LizardBle {
using CommandCallback = std::function<void(const std::string_view &)>;

/* Requires NVS to be initialized */
auto init(const std::string_view &deviceName,
          CommandCallback onCommand) -> void;
auto fini() -> void;
} // namespace LizardBle

#endif // ESP32LIZARDBLE_H
