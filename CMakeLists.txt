idf_component_register(
SRCS
    "include/esp32-lizard-ble.h"
    "esp32-lizard-ble.cpp"
INCLUDE_DIRS
    "include"
REQUIRES

PRIV_REQUIRES
    bt
    esp32-zauber
)
component_compile_options(-std=gnu++17 -Wsuggest-override)
