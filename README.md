# esp32-ble-command

Receive unformatted text commands via a single BLE service / characteristic pair.

# Example usage

```C++
#include <cstdio>
#include <string_view>
#include <nvs_flash.h>
#include <esp32-ble-command.h>

extern "C" {
auto app_main() -> void;
}

auto app_main() -> void {
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ZZ::BleCommand::init("MyDeviceName", [](const std::string_view &com) {
        std::printf("Command: [%.*s]\n", com.length(), com.data());
    });
}
```

## License

MIT
