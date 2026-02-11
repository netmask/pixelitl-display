# Pixelitl Display

A tiny pixel art display powered by WebAssembly. Load animated faces written in C, compiled to WASM, and watch them run on an 80x48 virtual framebuffer scaled 10x to an 800x480 LCD with a retro LED-matrix glow effect.

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

## Project Structure

```
pixelitl-display/
├── main/                       # ESP32 firmware
│   ├── main.c                  # App entry, dual-core tasks, face switching
│   ├── lcd.c                   # RGB LCD driver (no-fb bounce mode)
│   ├── touch.c                 # GT911 capacitive touch + gestures
│   ├── framebuffer.c           # Triple-buffered VFB + bounce scaling
│   ├── graphics.c              # Drawing primitives (circles, text, etc.)
│   ├── wasm_runtime.c          # wasm3 loader and frame executor
│   ├── wasm_host.c             # Host API bindings (40+ functions)
│   ├── native_face.c           # Native C plasma for benchmarking
│   ├── simd_ops.S              # ESP32-S3 SIMD (PIE) optimizations
│   ├── board.h                 # Pin definitions and constants
│   └── faces/*.wasm            # Embedded face binaries
├── sdk/
│   ├── pixelitl.h              # Face SDK header (include this)
│   └── examples/               # Example faces
│       ├── plasma.c            # Sine-based plasma effect
│       ├── clock.c             # Digital clock with seconds bar
│       ├── demo.c              # Multi-page animation showcase
│       └── Makefile            # WASM compilation rules
├── simulator/
│   └── pixelitl-simulator.html # Browser-based face simulator
└── components/
    └── wasm3/                  # wasm3 interpreter (git submodule)
```

## Writing a Face

A face is a single C file compiled to WebAssembly. It implements three functions:

```c
#include "pixelitl.h"

void app_init(void) {
    // Called once at load time
}

void app_update(unsigned int frame, unsigned int dt_ms) {
    // Called each frame with delta time in milliseconds
}

void app_draw(void) {
    // Draw to the 80x48 framebuffer
    clear(0x0000);
    fill_rect(10, 10, 20, 20, hsv(frame * 2.0f, 1.0f, 1.0f));
    draw_str(5, 40, "hello", rgb(255, 255, 255));
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

#### Color

| Function | Description |
|----------|-------------|
| `rgb(r, g, b)` | 8-bit RGB to RGB565 |
| `hsv(h, s, v)` | HSV to RGB565 (h: 0-360, s/v: 0.0-1.0) |
| `dim_color(color, factor)` | Scale brightness (factor: 0.0-1.0) |
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

#### System & Input

| Function | Description |
|----------|-------------|
| `width()`, `height()` | Returns 80, 48 |
| `millis()` | Milliseconds since boot |
| `get_frame()` | Current frame number |
| `log_print(str, len)` | Debug output to serial |
| `get_gesture()` | Consume gesture (tap, swipe, long press) |
| `get_touch_x()`, `get_touch_y()` | Touch position in VFB coords (-1 if none) |
| `is_pressed()` | 1 if finger down |

#### Convenience Helpers

```c
draw_str(x, y, "hello", color);       // auto-length draw_text
draw_str_big(x, y, "BIG", color);     // auto-length draw_text_big
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
cd sdk/examples
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install
```

This compiles all `.c` faces to `.wasm` and copies them to `main/faces/`.

### Build & Flash Firmware

```bash
source ~/esp/esp-idf/export.sh
idf.py build
idf.py flash
idf.py monitor    # view serial output
```

## Simulator

Open `simulator/pixelitl-simulator.html` in a browser. No build step required.

- **Load faces** via the "Load .wasm" button or drag-and-drop `.wasm` files onto the canvas
- **Switch faces** by clicking the tabs at the top
- **Touch input** works with mouse (click, drag, swipe gestures)
- **Rendering modes** selectable via dropdown: Flat, Grid (LED gap), Soft Glow

The simulator runs the same `.wasm` binaries as the ESP32, using the browser's native WebAssembly engine.

## Example Faces

### plasma.c
Procedural plasma effect using layered sine waves with HSV color cycling. Renders in 4x4 blocks for performance.

### clock.c
Digital clock displaying HH:MM:SS in large text with a blinking colon and seconds progress bar.

### demo.c
Multi-page animation showcase (swipe to navigate):
- Animated rectangles and easing curves
- Pulsing circles and orbiting dots
- Bouncing text with sequential fade-in
- HSV/RGB/lerp color gradient palettes
- Easing function comparison (quad, cubic, elastic, bounce)

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
