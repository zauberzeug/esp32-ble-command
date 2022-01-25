#ifndef ZZBLECOMMAND_H
#define ZZBLECOMMAND_H

#include <functional>
#include <string_view>

namespace ZZ::BleCommand {
using CommandCallback = std::function<void(const std::string_view &)>;

/* Requires NVS to be initialized */
auto init(const std::string_view &deviceName,
          CommandCallback onCommand) -> void;
auto fini() -> void;
} // namespace ZZ::BleCommand

#endif // ZZBLECOMMAND_H
