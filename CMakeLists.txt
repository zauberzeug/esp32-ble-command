idf_component_register(
SRCS
    "include/esp32-ble-command.h"
    "esp32-ble-command.cpp"
INCLUDE_DIRS
    "include"
REQUIRES

PRIV_REQUIRES
    bt
    esp32-zeug
)
component_compile_options(-std=gnu++17 -Wsuggest-override)
