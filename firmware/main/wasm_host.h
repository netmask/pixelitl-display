// WASM host function bindings (linked into wasm3 modules)
#pragma once

#include "wasm3.h"

extern const char *wasm_host_budget_trap;

void wasm_host_init(void);  // Initialize LUTs (call once at startup)
void wasm_host_link_all(IM3Module module);
void wasm_host_set_frame(uint32_t frame);
