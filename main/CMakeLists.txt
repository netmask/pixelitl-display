idf_component_register(
    SRCS
        "main.c"
        "lcd.c"
        "touch.c"
        "framebuffer.c"
        "graphics.c"
        "wasm_runtime.c"
        "wasm_host.c"
        "simd_ops.S"
        "wifi.c"
        "http_request.c"
        "kv_store.c"
    INCLUDE_DIRS "."
    REQUIRES wasm3
    PRIV_REQUIRES esp_timer esp_lcd esp_driver_i2c driver nvs_flash esp_wifi esp_netif esp_http_client mbedtls
)

# Embed faces into flash
target_add_binary_data(${COMPONENT_TARGET} "faces/store.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/glucose.wasm" BINARY)
