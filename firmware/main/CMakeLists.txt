idf_component_register(
    SRCS
        "main.c"
        "engine/wasm_runtime.c"
        "engine/wasm_host.c"
        "engine/framebuffer.c"
        "engine/graphics.c"
        "engine/simd_ops.S"
        "drivers/lcd.c"
        "drivers/touch.c"
        "drivers/wifi.c"
        "drivers/http_request.c"
        "drivers/kv_store.c"
        "drivers/esp_now_recv.c"
    INCLUDE_DIRS "." "engine" "drivers"
    REQUIRES wasm3
    PRIV_REQUIRES esp_timer esp_lcd esp_driver_i2c driver nvs_flash esp_wifi esp_netif esp_http_client mbedtls
)

# Embed faces into flash
target_add_binary_data(${COMPONENT_TARGET} "faces/store.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/glucose.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/dashboard.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/radar.wasm" BINARY)
