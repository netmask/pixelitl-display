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
    INCLUDE_DIRS "." "engine" "drivers" "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES wasm3
    PRIV_REQUIRES esp_timer esp_lcd esp_driver_i2c driver nvs_flash esp_wifi esp_netif esp_http_client mbedtls
)

# Read the embedded-face manifest. One face name per line, blank lines ignored.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/embedded_faces.txt" EMBEDDED_FACES)
list(REMOVE_ITEM EMBEDDED_FACES "")

# Generate embedded_faces.h next to other build outputs.
set(GEN_HEADER "${CMAKE_CURRENT_BINARY_DIR}/embedded_faces.h")

set(HDR "// Auto-generated from embedded_faces.txt — do not edit\n")
string(APPEND HDR "#pragma once\n#include <stdint.h>\n\n")

list(LENGTH EMBEDDED_FACES FACE_COUNT)
string(APPEND HDR "#define EMBEDDED_FACE_COUNT ${FACE_COUNT}\n\n")

foreach(NAME IN LISTS EMBEDDED_FACES)
    string(APPEND HDR "extern const uint8_t ${NAME}_wasm_start[] asm(\"_binary_${NAME}_wasm_start\");\n")
    string(APPEND HDR "extern const uint8_t ${NAME}_wasm_end[]   asm(\"_binary_${NAME}_wasm_end\");\n")
endforeach()

string(APPEND HDR "\nstatic const struct embedded_face_s {\n")
string(APPEND HDR "    const char *name;\n    const uint8_t *start;\n    const uint8_t *end;\n} EMBEDDED_FACES[] = {\n")
foreach(NAME IN LISTS EMBEDDED_FACES)
    string(APPEND HDR "    {\"${NAME}\", ${NAME}_wasm_start, ${NAME}_wasm_end},\n")
endforeach()
string(APPEND HDR "};\n")

file(GENERATE OUTPUT "${GEN_HEADER}" CONTENT "${HDR}")

# Embed each face binary from firmware/main/faces/<name>.wasm
foreach(NAME IN LISTS EMBEDDED_FACES)
    target_add_binary_data(${COMPONENT_TARGET} "faces/${NAME}.wasm" BINARY)
endforeach()
