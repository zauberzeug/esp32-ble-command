#ifndef ZZBLECOMMAND_H
#define ZZBLECOMMAND_H

#include <functional>
#include <string_view>

namespace ZZ::BleCommand {
using CommandCallback = std::function<void(const std::string_view &)>;

/* Requires NVS to be initialized.
 * Note that deviceName sent in the scan response may
 * at most be 29 bytes long, and will automatically be truncated.
 * The GAP attribute is unaffected by this limitation. */
auto init(const std::string_view &deviceName,
          CommandCallback onCommand) -> void;
auto fini() -> void;
} // namespace ZZ::BleCommand

#endif // ZZBLECOMMAND_H
