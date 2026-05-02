set(WASM3_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/wasm3/source")

file(GLOB WASM3_SRCS "${WASM3_SOURCE_DIR}/*.c")

idf_component_register(
    SRCS ${WASM3_SRCS}
    INCLUDE_DIRS "${WASM3_SOURCE_DIR}"
)

# Optimize interpreter for speed
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-parameter
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    d_m3HasFloat=1
    d_m3LogOutput=0
    M3_HAS_TAIL_CALL=0
)
