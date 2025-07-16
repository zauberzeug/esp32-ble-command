/*
 * SPDX-FileCopyrightText: 2022 Zauberzeug GmbH
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ZZ_BLE_COMMAND_H
#define ZZ_BLE_COMMAND_H

#include <cstdint>
#include <functional>
#include <string_view>

namespace ZZ::BleCommand {
using CommandCallback = std::function<void(const std::string_view &)>;
using PasskeyCallback = std::function<void(std::uint32_t passkey)>;
using AuthCompleteCallback = std::function<void(bool success, std::uint16_t conn_handle)>;

/* Requires NVS to be initialized.
 * Note that deviceName sent in the scan response may
 * at most be 29 bytes long, and will automatically be truncated.
 * The GAP attribute is unaffected by this limitation. */
auto init(const std::string_view &deviceName,
          CommandCallback onCommand) -> void;

/* Initialize with passkey authentication.
 * passkey: 6-digit PIN (000000-999999) to use for pairing
 * onPasskeyDisplay: optional callback when passkey should be displayed
 * onAuthComplete: optional callback when authentication completes */
auto init(const std::string_view &deviceName,
          CommandCallback onCommand,
          std::uint32_t passkey,
          PasskeyCallback onPasskeyDisplay = nullptr,
          AuthCompleteCallback onAuthComplete = nullptr) -> void;

/* Sends data to the first connected device via notification.
 * Returns 0 on success, NimBLE error code otherwise. */
auto send(const std::string_view &data) -> int;
auto fini() -> void;
} // namespace ZZ::BleCommand

#endif // ZZ_BLE_COMMAND_H
