# Pixelitl Display

A tiny pixel art display powered by WebAssembly. Write animated faces in C or AssemblyScript, compile to WASM, and run them on an 80x48 virtual framebuffer scaled 10x to an 800x480 LCD with a retro LED-matrix glow effect. Includes a web-based development studio for writing, compiling, and deploying faces over WiFi.

**Hardware:** Waveshare ESP32-S3-Touch-LCD-4.3 (dual-core 240 MHz, 8 MB PSRAM, 800x480 RGB LCD, capacitive touch)

**Runtime:** [wasm3](https://github.com/wasm3/wasm3) interpreter executing `.wasm` face binaries at 51 FPS

![Architecture](https://img.shields.io/badge/ESP--IDF-v6.1--dev-blue) ![WASM](https://img.shields.io/badge/runtime-wasm3-orange) ![Resolution](https://img.shields.io/badge/VFB-80%C3%9748-green)

---

## How It Works

```
  .wasm face          80x48 VFB            800x480 LCD
 +-----------+      +-----------+       +----------------+
 | app_draw()|----->| 7.5 KB    |--ISR->| bounce buffer  |
 | sinf()    |      | SRAM      |       | SRAM -> DMA    |
 | fill_rect |      | triple-buf|       | no PSRAM!      |
 +-----------+      +-----------+       +----------------+
   Core 1              Core 1              ISR (Core 0)
```

- **WASM faces** call host functions (draw, math, color) to paint into an 80x48 RGB565 framebuffer
- **Triple buffering** lets WASM produce frames without waiting for the display
- **Bounce buffer ISR** scales pixels 10x on-the-fly directly from SRAM VFB to SRAM bounce buffers — zero PSRAM traffic
- **Vsync-locked** frame production at the LCD refresh rate (~51 Hz with 21 MHz pixel clock)
- **Swipe left/right** on the touchscreen to switch between loaded faces
- **HD mode** (160x96) available for faces that need more resolution (5x scaling)

## Project Structure

```
pixelitl-display/
├── firmware/                   # ESP32-S3 display firmware (the engine)
│   ├── main/
│   │   ├── main.c              # app entry, dual-core tasks, face switching
│   │   ├── board.h             # pin definitions and constants
│   │   ├── embedded_faces.txt  # which app faces ship in flash (one name per line)
│   │   ├── faces/              # built .wasm binaries (gitignored, output of `make`)
│   │   ├── engine/             # WASM runtime, framebuffer, graphics, SIMD, font
│   │   │   ├── wasm_runtime.c  # wasm3 loader + frame executor + budget enforcer
│   │   │   ├── wasm_host.c     # host API bindings (40+ functions)
│   │   │   ├── framebuffer.c   # triple-buffered VFB + bounce scaling
│   │   │   ├── graphics.c      # drawing primitives
│   │   │   └── simd_ops.S      # ESP32-S3 SIMD (PIE) optimizations
│   │   └── drivers/            # peripherals + network
│   │       ├── lcd.c           # RGB LCD driver (no-fb bounce mode)
│   │       ├── touch.c         # GT911 capacitive touch + gestures
│   │       ├── wifi.c          # WiFi STA bring-up
│   │       ├── http_request.c  # HTTPS client
│   │       ├── kv_store.c      # NVS key-value store
│   │       └── esp_now_recv.c  # ESP-NOW receiver
│   ├── components/wasm3/       # wasm3 interpreter (git submodule)
│   ├── partitions.csv, sdkconfig*, CMakeLists.txt
│   └── ...                     # other IDF-managed files
├── sdk/                        # what apps link against
│   ├── pixelitl.h              # C SDK
│   ├── pixelitl.ts             # AssemblyScript SDK
│   └── face.mk                 # shared wasm32 build rule
├── apps/                       # real applications
│   ├── dashboard/              # face + Phoenix service (HN/sprite/dota)
│   │   ├── face/               # face source + Makefile
│   │   └── service/            # Phoenix backend, deployable to Fly.io
│   ├── glucose/face/           # glucose dashboard
│   ├── store/                  # face library browser (talks to studio)
│   ├── radar/                  # face + ESP32-WROOM peer
│   │   ├── face/
│   │   └── peer/               # IDF project: LD2450 mmWave + ESP-NOW broadcaster
│   └── weather/face/
├── demo/                       # engine showcase faces (toys)
│   ├── plasma/, clock/, nyan/, holi/, buttons/, invaders/, hd_demo/, ultra_demo/
│   └── pong/                   # AssemblyScript example
├── studio/                     # Phoenix LiveView dev tool
│   ├── lib/studio_web/live/    # editor, face library, simulator
│   └── lib/studio/compiler/    # C and AssemblyScript → WASM
├── tools/                      # rasterize_sprite.py, rasterize_stdin.py
├── docs/                       # design docs and implementation plans
└── Makefile                    # builds every face; recurses into apps/ + demo/
```

## Writing a Face

A face is a single C file (or AssemblyScript `.ts` file) compiled to WebAssembly. It implements three exported functions:

```c
#include "pixelitl.h"

void app_init(void) {
    // Called once at load time
    // Optional: set_resolution(1) for HD mode (160x96)
}

void app_update(unsigned int frame, unsigned int dt_ms) {
    // Called each frame with delta time in milliseconds
    // Handle input, update state
}

void app_draw(void) {
    // Draw to the framebuffer
    clear(0x0000);
    fill_rect(10, 10, 20, 20, hsv(get_frame() * 2.0f, 1.0f, 1.0f));
    draw_str(5, 40, "hello", rgb(255, 255, 255));
}
```

Optional callbacks:

```c
void app_wifi_ready(void) {
    // Called when WiFi connects — start HTTP requests here
}
```

### SDK API

#### Drawing

| Function | Description |
|----------|-------------|
| `clear(color)` | Fill entire framebuffer |
| `set_pixel(x, y, color)` | Set single pixel |
| `fill_rect(x, y, w, h, color)` | Filled rectangle |
| `draw_rect(x, y, w, h, color)` | Outlined rectangle |
| `draw_line_h(x, y, len, color)` | Horizontal line |
| `draw_line_v(x, y, len, color)` | Vertical line |
| `fill_circle(cx, cy, r, color)` | Filled circle |
| `draw_circle(cx, cy, r, color)` | Outlined circle |
| `draw_text(x, y, str, len, color)` | 5x7 pixel font |
| `draw_text_big(x, y, str, len, color)` | 10x14 pixel font (2x scaled) |
| `draw_str(x, y, str, color)` | Auto-length `draw_text` |
| `draw_str_big(x, y, str, color)` | Auto-length `draw_text_big` |

#### Color

| Function | Description |
|----------|-------------|
| `rgb(r, g, b)` | 8-bit RGB to RGB565 |
| `hsv(h, s, v)` | HSV to RGB565 (h: 0-360, s/v: 0.0-1.0) |
| `dim_color(color, factor)` | Scale brightness (0.0-1.0) |
| `lerp_color(c1, c2, t)` | Interpolate between two colors |

#### Math

| Function | Description |
|----------|-------------|
| `sinf(x)`, `cosf(x)` | Sine/cosine (512-entry LUT, fast) |
| `sqrtf(x)`, `absf(x)` | Square root, absolute value |
| `atan2f(y, x)` | Two-argument arctangent |
| `fmodf(x, y)` | Floating-point modulo |
| `rand()`, `srand(seed)` | Pseudo-random (0-32767) |

#### Easing (t: 0.0 to 1.0)

`ease_in_quad`, `ease_out_quad`, `ease_in_out_quad`, `ease_in_cubic`, `ease_out_cubic`, `ease_in_out_cubic`, `ease_out_elastic`, `ease_out_bounce`

#### System

| Function | Description |
|----------|-------------|
| `width()`, `height()` | Framebuffer dimensions (80x48 or 160x96) |
| `set_resolution(mode)` | 0 = standard (80x48), 1 = HD (160x96) |
| `millis()` | Milliseconds since boot |
| `get_frame()` | Current frame number |
| `log_print(str, len)` | Debug output to serial |
| `wifi_status()` | WiFi connection status |

#### Touch & Input

| Function | Description |
|----------|-------------|
| `is_pressed()` | 1 if finger is down |
| `get_touch_x()`, `get_touch_y()` | Touch position in VFB coords (-1 if none) |
| `get_gesture()` | Consume gesture event (returns `GESTURE_*` constant) |

Gesture constants: `GESTURE_NONE`, `GESTURE_TAP`, `GESTURE_SWIPE_LEFT`, `GESTURE_SWIPE_RIGHT`, `GESTURE_SWIPE_UP`, `GESTURE_SWIPE_DOWN`, `GESTURE_LONG_PRESS`

#### Button Widget

```c
pxl_button_t btn;
pxl_button(&btn, x, y, w, h, "Label", bg_color, fg_color);

// In app_update:
if (pxl_button_clicked(&btn)) { /* handle click */ }

// In app_draw:
pxl_button_draw(&btn);
```

#### HTTP (async)

```c
int handle = http_get_str("http://example.com/data");

// Poll in app_update:
int st = http_status(handle);
if (st == HTTP_DONE) {
    int code = http_response_code(handle);
    char buf[512];
    int len = http_read(handle, buf, sizeof(buf));
    http_close(handle);
}
```

Also available: `http_post(url, url_len, content_type, ct_len, body, body_len)`

#### KV Store (per-face persistent storage)

```c
kv_set_str("key", "value");

char buf[64];
int len = kv_get_str("key", buf, sizeof(buf));

kv_del_str("key");
```

#### Face Install (download & load faces at runtime)

```c
face_install(url, url_len, name, name_len);

// Poll with face_install_status():
// INSTALL_IDLE → INSTALL_DOWNLOADING → INSTALL_LOADING → INSTALL_OK / INSTALL_ERROR
```

## Building

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) v5.5+ (tested with v6.1-dev)
- [LLVM/Clang](https://llvm.org/) with wasm32 target (Apple Clang won't work)

```bash
# Install LLVM (macOS)
brew install llvm lld
```

### Compile WASM Faces

```bash
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make
```

This compiles every face under `apps/` and `demo/` to `.wasm` and copies them to `firmware/main/faces/`.

To control which faces ship in flash, edit `firmware/main/embedded_faces.txt` (one face name per line).

### Build & Flash Firmware

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py build
idf.py flash
idf.py monitor    # view serial output
```

### Build the Radar Peer (optional)

```bash
cd apps/radar/peer
idf.py build flash monitor -p /dev/cu.usbserial-...
```

### Run the Dashboard Service (optional)

```bash
cd apps/dashboard/service
mix deps.get
set -x OPENAI_API_KEY sk-...
mix run --no-halt
```

## Studio

Pixelitl Studio is a Phoenix LiveView web app for developing and deploying faces.

```bash
cd studio
mix setup
mix phx.server
```

Then open http://localhost:4000 in your browser.

- **Editor** — Write faces in C or AssemblyScript with syntax highlighting, compile to WASM, and preview in the built-in simulator
- **Faces** — Browse the face library, publish/unpublish faces
- **Simulator** — Run `.wasm` face binaries in the browser with multiple rendering modes (Flat, Grid, Soft Glow)

The device's **Store** face connects to the studio API to browse and install faces over WiFi. Configure the server URL via the `server` KV key (default: `http://192.168.1.124:4000/api/faces/`).

## Example Faces

| Face | Language | Description |
|------|----------|-------------|
| **plasma** | C | Procedural plasma using layered sine waves with HSV color cycling |
| **clock** | C | Digital HH:MM:SS clock with blinking colon and seconds bar |
| **demo** | C | Multi-page animation showcase (swipe to navigate) |
| **hd_demo** | C | HD mode (160x96) demo with finer detail |
| **buttons** | C | Touch button widget demonstration |
| **weather** | C | Weather display fetched via HTTP |
| **store** | C | Face store — browse and install faces from the studio over WiFi |
| **holi** | C | Psychedelic "HOLI" text with plasma background and sparkle particles |
| **nyan** | C | Nyan Cat pixel art animation with rainbow trail and twinkling stars |
| **pong** | AssemblyScript | Pong game with touch-controlled paddle and AI opponent |

## Performance

| Metric | Value |
|--------|-------|
| LCD refresh rate | ~51 Hz (21 MHz pixel clock) |
| WASM plasma frame time | ~4.5 ms |
| Native plasma frame time | ~1.2 ms |
| WASM overhead | ~4x vs native C |
| Frame budget | 19.5 ms (77% headroom with plasma) |
| VFB memory | 22.5 KB (3 x 7.5 KB triple buffer) |
| PSRAM usage for rendering | 0 bytes |

## License

MIT
