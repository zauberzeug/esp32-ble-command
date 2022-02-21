/*
 * SPDX-FileCopyrightText: 2022 Zauberzeug GmbH
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ZZ_BLE_COMMAND_H
#define ZZ_BLE_COMMAND_H

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

#endif // ZZ_BLE_COMMAND_H
