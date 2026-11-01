idf_component_register(
SRCS
    "ethernet.c"

INCLUDE_DIRS
    "include"

REQUIRES
    "esp_eth"
    "esp_netif"
    "esp_event"
    "esp_driver_spi"
    "esp_driver_gpio"
    "espressif__w5500"
)
