# Pixelitl Display — Architecture & Technical Reference

A comprehensive technical document describing the Pixelitl retro fantasy console: an ESP32-S3 device that runs sandboxed WebAssembly "faces" on a pixel-art display with an LED-matrix aesthetic.

---

## 1. Overview

Pixelitl is an embedded WebAssembly platform that turns a 4.3" 800×480 touchscreen into a retro pixel-art display. Each app ("face") is a small WASM module (typically 1–15 KB) that targets a low-resolution virtual framebuffer (default 80×48). The firmware scales the VFB 10× to the physical LCD and applies a per-pixel glow effect to mimic an LED matrix.

**Design goals**

- **Safe sandboxing** — faces run inside wasm3 with a per-frame execution budget; misbehaving faces are throttled or killed without crashing the firmware.
- **Hot-reload over WiFi** — the companion Studio (Phoenix LiveView) lets you edit C or AssemblyScript in the browser, compile to WASM, and push to the device.
- **Retro aesthetic** — low-res VFB + LED-glow scaling produces a distinctive look.
- **High throughput** — triple-buffered rendering, vsync-synchronized frame production, and SRAM-only data paths achieve ~51 FPS.

---

## 2. Hardware

| Component | Detail |
|-----------|--------|
| Board | Waveshare ESP32-S3-Touch-LCD-4.3 |
| CPU | Dual-core Xtensa LX7 @ 240 MHz |
| RAM | 384 KB internal SRAM + 8 MB PSRAM |
| Flash | 16 MB (2 MB firmware, ~2 MB SPIFFS, NVS) |
| Display | 800×480 RGB565, parallel RGB, 21 MHz pixel clock (~51 Hz) |
| Touch | GT911 capacitive, I²C |
| Wireless | WiFi 802.11 b/g/n, BLE |

Pin assignments live in `main/board.h`.

---

## 3. Repository Layout

```
pixelitl-display/
├── main/                 # ESP32 firmware
│   ├── main.c            # Boot, FreeRTOS tasks, core pinning
│   ├── lcd.c             # RGB LCD driver (bounce-buffer mode)
│   ├── touch.c           # GT911 driver + gesture detection
│   ├── framebuffer.c     # Triple-buffered VFB + vsync swap + glow scaler
│   ├── graphics.c        # Drawing primitives & color helpers
│   ├── wasm_runtime.c    # wasm3 loader, face lifecycle, budget enforcement
│   ├── wasm_host.c       # Host functions imported by faces
│   ├── wifi.c            # WiFi station bring-up
│   ├── http_request.c    # Async HTTP client
│   ├── kv_store.c        # Per-face NVS key/value store
│   ├── simd_ops.S        # Xtensa PIE 128-bit SIMD fill/copy
│   ├── board.h           # Pins + resolution constants
│   └── faces/            # Embedded .wasm binaries (glucose, store)
├── components/
│   └── wasm3/            # wasm3 interpreter (submodule)
├── sdk/
│   ├── pixelitl.h        # C SDK for face authors
│   ├── pixelitl.ts       # AssemblyScript SDK
│   └── examples/         # Example faces + Makefile
├── studio/               # Phoenix LiveView development tool (editor + simulator)
├── simulator/            # Standalone HTML simulator
├── partitions.csv        # Flash partition table
└── CMakeLists.txt        # Top-level ESP-IDF project
```

---

## 4. Firmware Architecture

### 4.1 Boot flow (`main/main.c`)

1. Init NVS and per-face KV store.
2. Init framebuffer, WASM host, LCD, touch, WASM runtime.
3. Embed and load the built-in faces (`glucose` default, `store` for installing new faces).
4. Spawn two pinned FreeRTOS tasks:
   - **Core 0** — touch polling
   - **Core 1** — WASM frame executor (96 KB stack in PSRAM)
5. Start WiFi + HTTP client; the main task logs periodic statistics.

### 4.2 Dual-core task model

**Core 1 — WASM executor** (`wasm_task`)

```
loop:
  fb_wait_vsync()              # blocks on semaphore signaled by LCD ISR
  check install completion     # commits newly downloaded faces
  wasm_execute_frame(face, frame, dt_ms)
    → app_update()
    → app_draw()               # writes to back VFB
  fb_publish()                 # swap ready ↔ back
```

**Core 0 — touch poll** (`touch_task`)

```
loop:
  touch_poll()                 # GT911 I2C read
  consume SWIPE_LEFT/RIGHT     # cycle active face
  sleep 10 ms
```

Vsync and bounce-buffer fill run in LCD hardware ISRs.

### 4.3 Rendering pipeline

Frames flow through three stages with **zero PSRAM traffic**:

1. **WASM writes** to the back VFB in SRAM (RGB565).
2. **`fb_publish()`** atomically swaps ready ↔ back (triple buffered, spinlock-protected).
3. **Vsync ISR** (`fb_vsync_swap`) promotes ready → front and signals the WASM semaphore.
4. **Bounce-empty ISR** (`fb_fill_bounce`) scales the VFB into the LCD's bounce buffer on-the-fly with the glow effect applied.
5. **DMA** streams the bounce buffer to the panel at 21 MHz.

Triple buffering decouples WASM FPS from display refresh — faces never stall on vsync.

### 4.4 Resolutions

Defined in `board.h`. Faces opt in via `set_resolution()` in `app_init()`:

| Mode | VFB | Scale | Glow pattern |
|------|-----|-------|--------------|
| `RES_STANDARD` | 80×48 | 10× | 9 bright + 1 dim right; bottom row dim |
| `RES_HD` | 160×96 | 5× | reduced glow border |
| `RES_ULTRA` | 200×120 | 4× | reduced glow border |

VFB buffers are allocated at Ultra size (48 KB × 3 = 144 KB) so switching modes is free.

### 4.5 Glow effect

Each VFB pixel expands to a 10×10 block in the LCD output. The rightmost column and bottom row are dimmed (`GLOW_DIM_FACTOR = 100/255 ≈ 39%`), producing a subtle shadow that evokes LEDs on a matrix. Implemented in the bounce-fill ISR (`framebuffer.c`).

### 4.6 SIMD acceleration

`simd_ops.S` provides hand-written Xtensa PIE 128-bit routines:

- `simd_fill16` — 8 pixels per instruction fill (used by `gfx_clear`)
- `simd_memcpy` — aligned 128-bit copy

Q registers are unmanaged by the compiler, and `LOOPNEZ` must live inside the asm block.

---

## 5. WASM Sandbox

### 5.1 Lifecycle (`wasm_runtime.c`)

```
m3_ParseModule  →  m3_LoadModule  →  wasm_host_link_all  →  find exports  →  app_init()
```

Required exports: `app_init`, `app_update(frame, dt_ms)`, `app_draw`.
Optional exports: `app_on_gesture(g)`, `app_wifi_ready`, `app_destroy`.

Memory: 128 KB initial, 256 KB max (linker flags in `sdk/examples/Makefile`).

### 5.2 Budget enforcement

Default budget: **19.5 ms/frame** (51 Hz target).

State machine per face:

| State | Trigger | Effect |
|-------|---------|--------|
| RUNNING | — | normal execution |
| WARNED | 1 overage | log warning |
| THROTTLED | 30 consecutive overages | reduce update rate |
| KILLED | 300 consecutive overages | unload face |

Execution is trapped via a sentinel string (`wasm_host_budget_trap`) returned from host calls when the budget is exceeded.

### 5.3 Host API (wasm_host.c)

Exposed to faces via `sdk/pixelitl.h` / `sdk/pixelitl.ts`:

- **Graphics** — `clear`, `set_pixel`, `fill_rect`, `draw_rect`, `draw_line_h/v`, `draw_circle`, `fill_circle`, `draw_text`, `draw_text_big`, `blit_framebuffer`, `blit_rect`
- **Color** — `rgb`, `hsv`, `dim_color`, `lerp_color`
- **Math** — `sinf`/`cosf` (512-entry LUT + linear interpolation), `sqrtf`, `atan2f`, `fmodf`, `absf`, `rand`/`srand`
- **Easing** — quad, cubic, elastic, bounce variants
- **System** — `millis` (esp_timer), `width`, `height`, `get_frame`, `log_print`, `set_resolution`
- **Touch** — `is_pressed`, `get_touch_x/y`, `get_gesture`
- **WiFi/HTTP** — `wifi_status`, `http_get`/`post`, `http_status`, `http_response_*`, `http_read`, `http_close`
- **KV store** — `kv_set`, `kv_get`, `kv_del` (NVS-backed, per face namespace)
- **Face management** — `face_install(url, name)`, `face_install_status`, `face_count`, `face_switch`, `face_get_name`

### 5.4 Building a face

`sdk/examples/Makefile`:

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin \
  -Wl,--no-entry \
  -Wl,--export=app_init -Wl,--export=app_update -Wl,--export=app_draw \
  -Wl,--allow-undefined \
  -Wl,--initial-memory=131072 -Wl,--max-memory=262144
```

Apple Clang does not target `wasm32`; install Homebrew `llvm` + `lld`:

```
brew install llvm lld
cd sdk/examples && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install
```

`make install` copies every built `.wasm` into `main/faces/`.

### 5.5 Dynamic face installation

The built-in `store` face queries the Studio API and downloads faces over HTTP:

1. `face_install(url, name)` kicks off an async GET (non-blocking; buffered to PSRAM).
2. Main frame loop polls `wasm_install_status()` between frames — rendering continues unaffected.
3. On completion, `wasm_install_commit()` parses the module and hot-adds it.
4. User swipes left/right to cycle active faces.

---

## 6. Graphics Subsystem

- **Color format** — RGB565 end-to-end (VFB, bounce buffer, panel).
- **Font** — 5×7 bitmap (`font5x7.h`); `draw_text_big` scales 2× to 10×14.
- **Circles** — midpoint (Bresenham) algorithm.
- **HSV→RGB** — sector-based conversion with full saturation support.
- **Frame pack** — two RGB565 pixels packed per uint32 for 32-bit writes on aligned runs.

---

## 7. Example Faces (`sdk/examples/`)

| File | Size | Description |
|------|------|-------------|
| `plasma.c` | 805 B | Sine-layered plasma with HSV cycling |
| `clock.c` | 1.2 KB | HH:MM:SS digital clock with blinking colon |
| `demo.c` | 3.8 KB | Multi-page animation showcase (swipe pages) |
| `hd_demo.c` | 3.2 KB | 160×96 HD mode demo |
| `ultra_demo.c` | 2.5 KB | 200×120 Ultra mode demo |
| `buttons.c` | 4.3 KB | Touch button widgets |
| `weather.c` | 5.3 KB | JSON weather via HTTP |
| `pong.ts` | 4.9 KB | AssemblyScript Pong with AI opponent |
| `invaders.c` | 15 KB | Space Invaders-style shooter |
| `holi.c` | 1.6 KB | "HOLI" text with plasma + sparkles |
| `nyan.c` | 3.0 KB | Nyan Cat animation with rainbow trail |

Embedded at build time: `faces/glucose.wasm` (default boot face) and `faces/store.wasm` (installer).

---

## 8. Studio (Development Companion)

`studio/` is a Phoenix LiveView app providing:

- **Editor** — Monaco editor for C and AssemblyScript with server-side compilation (clang / asc).
- **Browser simulator** — loads the same `.wasm` binaries the device runs; implements host functions against a Canvas 2D context. Supports flat / grid / soft-glow rendering modes.
- **Face library** — publish/unpublish faces for device discovery via the `store` face.
- **Device push** — deliver `.wasm` over HTTP directly to an online device.

The standalone `simulator/pixelitl-simulator.html` is a dependency-free local variant.

---

## 9. Build & Flash

### Firmware

```fish
source ~/esp/esp-idf/export.sh
idf.py build flash monitor
```

Toolchain pin: ESP-IDF **v6.1-dev** at `~/esp/esp-idf`. Key compatibility notes:

- wasm3 on Xtensa: define `M3_HAS_TAIL_CALL=0` (not `d_m3NoTailCalls`) to disable `musttail`.
- RGB LCD API in v6 removes `bits_per_pixel`/`sram_trans_align`/`psram_trans_align`; use `in_color_format`/`out_color_format` + `dma_burst_size`.
- Use the new `driver/i2c_master.h` (legacy `driver/i2c.h` is EOL in v6).
- `esp_timer` must be in `main/CMakeLists.txt` `PRIV_REQUIRES`.
- `ESP_RETURN_ON_ERROR` needs `#include "esp_check.h"`.

ESP-DSP is **not** used — it lacks RGB565 ops, and `dsps_mulc_s16` bleeds channels. Manual SIMD is faster for this workload.

### Faces

```fish
cd sdk/examples
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install
```

---

## 10. Partition Layout (`partitions.csv`)

| Name | Type | Offset | Size |
|------|------|--------|------|
| `nvs` | data / nvs | 0x9000 | 24 KB (per-face KV) |
| `phy_init` | data / phy | 0xF000 | 4 KB |
| `factory` | app | 0x10000 | 2 MB |
| `spiffs` | data / spiffs | 0x210000 | 1984 KB (assets / face cache) |

---

## 11. Performance Notes

- **Frame budget** — 19.5 ms/face, hard-enforced.
- **Measured WASM frame** — ~4.5 ms for plasma at 80×48.
- **Panel refresh** — ~51 Hz at 21 MHz pixel clock (up from 39 Hz at 16 MHz).
- **Memory** — VFB + bounce buffer + WASM stacks ≈ 304 KB internal SRAM. PSRAM is used only for WASM task stacks and HTTP buffers.
- **Zero PSRAM traffic on render path** — VFB and bounce buffer both live in SRAM; the LCD DMA reads straight out of SRAM.

---

## 12. Extending the System

Common extensions:

- **New host function** — add the C implementation in `wasm_host.c`, register it in `wasm_host_link_all`, mirror the signature in `sdk/pixelitl.h`.
- **New face** — drop a `.c` or `.ts` file in `sdk/examples/`, declare it in the Makefile, run `make install`.
- **Persistent state** — use `kv_set`/`kv_get`; NVS namespace is per-face.
- **Network data** — use the async HTTP API; never block in `app_update`/`app_draw`.

---

## 13. References

- wasm3 — https://github.com/wasm3/wasm3
- ESP-IDF — https://github.com/espressif/esp-idf
- Waveshare board — https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm
- AssemblyScript — https://www.assemblyscript.org/
