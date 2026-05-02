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

Pin assignments live in `firmware/main/board.h`.

---

## 3. Repository Layout

```
pixelitl-display/
├── firmware/                 # ESP32-S3 display firmware (the engine)
│   ├── main/
│   │   ├── main.c            # Boot, FreeRTOS tasks, core pinning
│   │   ├── board.h           # Pins + resolution constants
│   │   ├── embedded_faces.txt  # Manifest of which app faces ship in flash
│   │   ├── faces/            # Built .wasm binaries (gitignored)
│   │   ├── engine/           # WASM execution platform
│   │   │   ├── wasm_runtime.c  # wasm3 loader, face lifecycle, budget
│   │   │   ├── wasm_host.c     # Host functions imported by faces
│   │   │   ├── framebuffer.c   # Triple-buffered VFB + vsync + glow scaler
│   │   │   ├── graphics.c      # Drawing primitives & color helpers
│   │   │   └── simd_ops.S      # Xtensa PIE 128-bit SIMD fill/copy
│   │   └── drivers/          # Peripherals + network
│   │       ├── lcd.c           # RGB LCD driver (bounce-buffer mode)
│   │       ├── touch.c         # GT911 driver + gesture detection
│   │       ├── wifi.c          # WiFi station bring-up
│   │       ├── http_request.c  # Async HTTP client
│   │       ├── kv_store.c      # Per-face NVS key/value store
│   │       └── esp_now_recv.c  # ESP-NOW receiver
│   ├── components/wasm3/     # wasm3 interpreter (submodule)
│   ├── partitions.csv        # Flash partition table
│   ├── sdkconfig*            # IDF config
│   └── CMakeLists.txt        # IDF top-level
├── sdk/
│   ├── pixelitl.h            # C SDK for face authors
│   ├── pixelitl.ts           # AssemblyScript SDK
│   └── face.mk               # Shared wasm32 build rule
├── apps/                     # Real applications (each is one product)
│   ├── dashboard/{face,service}/  # face + Phoenix backend (HN/sprite/dota)
│   ├── glucose/face/
│   ├── store/face/
│   ├── radar/{face,peer}/         # face + ESP32-WROOM peer (LD2450 mmWave)
│   └── weather/face/
├── demo/                     # Engine showcase faces (toys)
│   └── plasma/, clock/, nyan/, holi/, buttons/, invaders/, hd_demo/, ultra_demo/, pong/
├── studio/                   # Phoenix LiveView dev tool (editor + simulator)
├── tools/                    # rasterize_sprite.py, rasterize_stdin.py
├── docs/                     # Design docs and implementation plans
└── Makefile                  # Walks apps/ + demo/ to build every face
```

---

## 4. Firmware Architecture

### 4.1 Boot flow (`firmware/main/main.c`)

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

Memory: 128 KB initial, 256 KB max (linker flags in `sdk/face.mk`).

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

Each app/demo has a 2-line `Makefile` that includes the shared `sdk/face.mk`:

```makefile
NAME = my_face
include ../../../sdk/face.mk     # apps/<name>/face/Makefile
# or
include ../../sdk/face.mk        # demo/<name>/Makefile
```

`sdk/face.mk` runs:

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
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make    # from repo root
```

The root `Makefile` walks every `apps/*/face` and `demo/*` directory and recurses with the `install` target, copying every built `.wasm` into `firmware/main/faces/`.

To control which faces ship in flash, edit `firmware/main/embedded_faces.txt` (one face name per line). The firmware build reads it and embeds the matching binaries from `firmware/main/faces/`.

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

## 7. Apps and Demos

### Real applications (`apps/`)

| App | Parts | Description |
|-----|-------|-------------|
| `dashboard` | face + service | HN headlines + glucose + DOTA alert; service on Fly.io provides feeds, sprite generation, and alert state. |
| `glucose` | face | Glucose monitor (Dexcom). |
| `store` | face | Browse and install faces from the studio over WiFi. |
| `radar` | face + peer | Visualizes ESP-NOW packets from the LD2450 mmWave peer. |
| `weather` | face | JSON weather over HTTP. |

### Engine showcase faces (`demo/`)

| File | Description |
|------|-------------|
| `plasma` | Sine-layered plasma with HSV cycling |
| `clock` | HH:MM:SS digital clock with blinking colon |
| `hd_demo` | 160×96 HD mode demo |
| `ultra_demo` | 200×120 Ultra mode demo |
| `buttons` | Touch button widgets |
| `invaders` | Space Invaders-style shooter |
| `holi` | "HOLI" text with plasma + sparkles |
| `nyan` | Nyan Cat animation with rainbow trail |
| `pong` | AssemblyScript Pong with AI opponent |

Embedded at build time: whichever app faces are listed in `firmware/main/embedded_faces.txt` (today: `dashboard`, `glucose`, `store`, `radar`).

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
cd firmware
idf.py build flash monitor
```

Toolchain pin: ESP-IDF **v6.1-dev** at `~/esp/esp-idf`. Key compatibility notes:

- wasm3 on Xtensa: define `M3_HAS_TAIL_CALL=0` (not `d_m3NoTailCalls`) to disable `musttail`.
- RGB LCD API in v6 removes `bits_per_pixel`/`sram_trans_align`/`psram_trans_align`; use `in_color_format`/`out_color_format` + `dma_burst_size`.
- Use the new `driver/i2c_master.h` (legacy `driver/i2c.h` is EOL in v6).
- `esp_timer` must be in `firmware/main/CMakeLists.txt` `PRIV_REQUIRES`.
- `ESP_RETURN_ON_ERROR` needs `#include "esp_check.h"`.

ESP-DSP is **not** used — it lacks RGB565 ops, and `dsps_mulc_s16` bleeds channels. Manual SIMD is faster for this workload.

### Faces

```fish
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make    # from repo root, builds every face
```

Per-app/demo Makefiles each include `sdk/face.mk`. To embed a face into firmware, add its name to `firmware/main/embedded_faces.txt`.

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

- **New host function** — add the C implementation in `firmware/main/engine/wasm_host.c`, register it in `wasm_host_link_all`, mirror the signature in `sdk/pixelitl.h`.
- **New face / app** — `mkdir apps/<name>/face && touch apps/<name>/face/<name>.c apps/<name>/face/Makefile` (with `NAME = <name>` and `include ../../../sdk/face.mk`). Run `make` from repo root. To ship it embedded, add `<name>` to `firmware/main/embedded_faces.txt`.
- **New demo** — same as above but under `demo/<name>/` with `include ../../sdk/face.mk`. Demos are not normally embedded.
- **Persistent state** — use `kv_set`/`kv_get`; NVS namespace is per-face.
- **Network data** — use the async HTTP API; never block in `app_update`/`app_draw`.

---

## 13. References

- wasm3 — https://github.com/wasm3/wasm3
- ESP-IDF — https://github.com/espressif/esp-idf
- Waveshare board — https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm
- AssemblyScript — https://www.assemblyscript.org/
