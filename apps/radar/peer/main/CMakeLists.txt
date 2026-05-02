idf_component_register(
    SRCS "main.c" "ld2450.c" "espnow_tx.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer esp_hw_support spi_flash esp_driver_uart
                  nvs_flash esp_wifi esp_netif esp_event
)
