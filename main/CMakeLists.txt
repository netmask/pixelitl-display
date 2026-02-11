idf_component_register(
    SRCS
        "main.c"
        "lcd.c"
        "touch.c"
        "framebuffer.c"
        "graphics.c"
        "wasm_runtime.c"
        "wasm_host.c"
        "native_face.c"
        "simd_ops.S"
    INCLUDE_DIRS "."
    REQUIRES wasm3
    PRIV_REQUIRES esp_timer esp_lcd esp_driver_i2c driver
)

# Embed WASM face binaries
target_add_binary_data(${COMPONENT_TARGET} "faces/plasma.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/clock.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/demo.wasm" BINARY)
