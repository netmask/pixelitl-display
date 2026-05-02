# Pixelitl WASM3 Runtime — Design Document

## Overview

Pixelitl is a pixel art display running on the Waveshare ESP32-S3-Touch-LCD-4.3 (800×480 LCD). It renders an 80×48 virtual framebuffer and scales it up with bloom/glow effects to look like real LED hardware.

This document specifies the WASM3 runtime layer that lets users write, compile, and load custom apps (called **faces**) as `.wasm` binaries. Faces can be loaded from flash, SD card, or HTTP — and hot-swapped at runtime without rebooting.

An HTML simulator runs the same face code in the browser for instant previewing.

---

## Goals

1. **Users write faces in C, Rust, Zig, or AssemblyScript** — compile to `.wasm`, drop onto the device
2. **60-80% of frame budget stays native** — WASM only does logic + draw calls, native C does scale-up, glow, DMA
3. **No face can crash the system** — sandboxed memory, enforced time budgets, graceful degradation
4. **Simulator parity** — the browser simulator runs identical face `.wasm` binaries via the browser's native WASM engine
5. **Hot reload** — swap faces over WiFi without reflashing firmware

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ESP32-S3 (240MHz, 8MB PSRAM)                 │
│                                                                      │
│  ┌─────────────── Core 0 ──────────────┐  ┌──── Core 1 ──────────┐ │
│  │                                     │  │                       │ │
│  │  Render Pipeline (native C)         │  │  WASM Executor        │ │
│  │  ┌───────────────────────────────┐  │  │  ┌─────────────────┐ │ │
│  │  │ 1. Read front framebuffer     │  │  │  │ 1. Budget timer  │ │ │
│  │  │ 2. Draw system chrome (dots)  │  │  │  │ 2. app_update()  │ │ │
│  │  │ 3. Transition blend (if any)  │  │  │  │ 3. Clear back fb │ │ │
│  │  │ 4. Scale 80×48 → 800×480     │  │  │  │ 4. app_draw()    │ │ │
│  │  │ 5. Apply glow kernel          │  │  │  │ 5. Swap buffers  │ │ │
│  │  │ 6. DMA push to LCD            │  │  │  │    (if on time)  │ │ │
│  │  └───────────────────────────────┘  │  │  └─────────────────┘ │ │
│  │                                     │  │                       │ │
│  │  Touch Polling (GT911 I2C)          │  │  WiFi Face Loader     │ │
│  │  Gesture → queue → WASM executor    │  │  HTTP → parse → hot   │ │
│  │                                     │  │         swap          │ │
│  └─────────────────────────────────────┘  └───────────────────────┘ │
│                                                                      │
│  ┌─────────────── Shared Memory ───────────────────────────────────┐ │
│  │  Per-face: virt_fb_front[3840] ←→ virt_fb_back[3840]           │ │
│  │  Gesture mailbox: gesture_t + touch_x + touch_y (atomic)       │ │
│  │  Face registry: name, state, budget, stats                      │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

```
┌──────────────────────────────────────────────────────────────────────┐
│                     Browser Simulator                                │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  Same .wasm binary loaded via WebAssembly.instantiate()       │  │
│  │  Same host API (clear, set_pixel, fill_rect, draw_text, ...)  │  │
│  │  JavaScript implements host functions against a Canvas         │  │
│  │  requestAnimationFrame drives update/draw loop                │  │
│  │  Mouse/touch → gesture detection → same gesture API           │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌──────────┐  ┌────────────┐  ┌──────────┐  ┌────────────────┐    │
│  │ Face     │  │ Glow Mode  │  │ Virtual  │  │ Performance    │    │
│  │ Selector │  │ Toggle     │  │ Res Ctrl │  │ Overlay        │    │
│  └──────────┘  └────────────┘  └──────────┘  └────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Frame Lifecycle

### ESP32

```
Time (ms)  Core 0 (Render)              Core 1 (WASM)
────────── ──────────────────────────── ──────────────────────────
 0.0       Read front_fb                Start budget timer
 0.5       Scale row 0-9                Call app_update(frame, 33)
 1.0       Scale row 10-19              ↓ (wasm3 interpreter)
 ...       ...                          ↓
 3.0       Scale row 40-47              app_update returns
 3.5       Apply glow kernel            Clear back_fb
 8.0       DMA push starts              Call app_draw()
 8.5       ↓ (DMA running)             ↓ host: clear()
 9.0       ↓                           ↓ host: fill_rect() ×20
10.0       DMA done                     ↓ host: draw_text() ×5
10.5       Idle / touch poll            ↓ host: set_pixel() ×200
12.0       ↓                           app_draw returns (12ms total)
12.5       ↓                           Swap front ↔ back
           ↓                           Sleep until next tick
33.0       Next frame starts            Next frame starts
```

### Browser Simulator

```
requestAnimationFrame(loop)
  ├── face.app_update(frame, dt)     // WASM (browser-native speed)
  ├── clear back buffer
  ├── face.app_draw()                // WASM → JS host functions
  ├── renderToCanvas()               // JS: scale + glow to Canvas
  └── putImageData()
```

---

## Host API Specification

These are the functions that WASM faces can call. The API is identical on ESP32 and in the browser simulator.

### Graphics

| Function | Signature | Description |
|---|---|---|
| `clear` | `(color: u32) → void` | Fill entire 80×48 fb with color |
| `set_pixel` | `(x: i32, y: i32, color: u32) → void` | Set one pixel (bounds-checked) |
| `fill_rect` | `(x: i32, y: i32, w: i32, h: i32, color: u32) → void` | Filled rectangle |
| `draw_rect` | `(x: i32, y: i32, w: i32, h: i32, color: u32) → void` | Outlined rectangle (1px border) |
| `draw_line_h` | `(x: i32, y: i32, len: i32, color: u32) → void` | Horizontal line |
| `draw_line_v` | `(x: i32, y: i32, len: i32, color: u32) → void` | Vertical line |
| `draw_circle` | `(cx: i32, cy: i32, r: i32, color: u32) → void` | Outlined circle |
| `fill_circle` | `(cx: i32, cy: i32, r: i32, color: u32) → void` | Filled circle |
| `draw_text` | `(x: i32, y: i32, ptr: i32, len: i32, color: u32) → i32` | 5×7 font, returns pixel width |
| `draw_text_big` | `(x: i32, y: i32, ptr: i32, len: i32, color: u32) → i32` | 10×14 (2× scale), returns width |

All coordinates are in virtual pixel space (0-79 horizontal, 0-47 vertical). Out-of-bounds writes are silently ignored.

Colors are RGB565 packed into the low 16 bits of a u32:

```
Bit layout: RRRRR GGGGGG BBBBB
            15-11  10-5   4-0
```

### Color Utilities

| Function | Signature | Description |
|---|---|---|
| `rgb` | `(r: i32, g: i32, b: i32) → u32` | 8-bit RGB → RGB565 |
| `hsv` | `(h: f32, s: f32, v: f32) → u32` | HSV (h: 0-360, s: 0-1, v: 0-1) → RGB565 |
| `dim_color` | `(color: u32, factor: f32) → u32` | Multiply brightness (0.0-1.0) |
| `lerp_color` | `(c1: u32, c2: u32, t: f32) → u32` | Linear interpolate between colors |

### Math

| Function | Signature | Description |
|---|---|---|
| `sinf` | `(x: f32) → f32` | Sine (radians) |
| `cosf` | `(x: f32) → f32` | Cosine |
| `sqrtf` | `(x: f32) → f32` | Square root |
| `atan2f` | `(y: f32, x: f32) → f32` | Arc tangent |
| `fmodf` | `(x: f32, y: f32) → f32` | Float modulus |
| `absf` | `(x: f32) → f32` | Absolute value |
| `rand` | `() → i32` | Random 0-32767 |
| `srand` | `(seed: i32) → void` | Seed RNG |

**Rationale**: WASM has `f32.add`, `f32.mul`, etc. as native instructions, but `sin`, `cos`, `sqrt` are NOT wasm opcodes. In a bare wasm module (no libc), these would need software implementations that run in the interpreter — extremely slow. By providing them as host imports, they execute as native ARM `sinf()` calls. A plasma effect calling `sinf` 3,840 times per frame goes from ~40ms (software) to ~1ms (native).

### System

| Function | Signature | Description |
|---|---|---|
| `millis` | `() → i64` | Milliseconds since boot |
| `width` | `() → i32` | Virtual framebuffer width (80) |
| `height` | `() → i32` | Virtual framebuffer height (48) |
| `log_print` | `(ptr: i32, len: i32) → void` | Print to serial/console (debug) |
| `get_frame` | `() → i32` | Current frame number |

### Touch / Input

| Function | Signature | Description |
|---|---|---|
| `get_gesture` | `() → i32` | Last gesture since prev frame (consumed on read) |
| `get_touch_x` | `() → i32` | Touch X in virtual coords (-1 if none) |
| `get_touch_y` | `() → i32` | Touch Y in virtual coords (-1 if none) |
| `is_pressed` | `() → i32` | 1 if finger currently touching, 0 if not |

Gesture constants:

```
GESTURE_NONE        = 0
GESTURE_TAP         = 1
GESTURE_SWIPE_LEFT  = 2
GESTURE_SWIPE_RIGHT = 3
GESTURE_SWIPE_UP    = 4
GESTURE_SWIPE_DOWN  = 5
GESTURE_LONG_PRESS  = 6
```

### Storage (Persistent Key-Value)

| Function | Signature | Description |
|---|---|---|
| `storage_get` | `(key_ptr: i32, key_len: i32, val_ptr: i32, max_len: i32) → i32` | Read value, returns length (-1 if not found) |
| `storage_set` | `(key_ptr: i32, key_len: i32, val_ptr: i32, val_len: i32) → i32` | Write value (0=ok, -1=err) |
| `storage_del` | `(key_ptr: i32, key_len: i32) → i32` | Delete key |

Storage is per-face (namespaced by face name). Max key: 32 bytes. Max value: 256 bytes. Backed by NVS on ESP32, localStorage in simulator.

### Network (Optional, ESP32 only)

| Function | Signature | Description |
|---|---|---|
| `http_get` | `(url_ptr: i32, url_len: i32, buf_ptr: i32, max_len: i32) → i32` | Fetch URL, returns body length (-1=err) |
| `http_status` | `() → i32` | HTTP status code of last request |

Network calls are **blocking** and eat into the frame budget. Faces should call these sparingly (once per minute, not per frame) and cache results in memory.

In the simulator, `http_get` is implemented via `fetch()` with a CORS proxy option.

---

## Face Module Contract

Every `.wasm` face must export exactly these three functions:

```c
void app_init(void);                  // Called once when face loads
void app_update(int frame, int dt);   // Called every frame: logic, state
void app_draw(void);                  // Called every frame: render
```

Optionally:

```c
void app_on_gesture(int gesture, int x, int y);  // Called on gesture (alternative to polling)
void app_destroy(void);                            // Called before unload
```

### Memory

- WASM linear memory: 1 page (64KB) initial, up to 4 pages (256KB) max
- Stack: 16KB (inside the linear memory)
- No heap allocator provided — faces manage their own memory within the linear memory, or use a bump allocator
- The host never reads/writes WASM memory except through explicit API calls (`draw_text` reads a string pointer, `storage_get` writes to a buffer pointer)

### Compilation

**C** (recommended for small faces):
```fish
clang --target=wasm32 -O2 -nostdlib \
  -Wl,--no-entry \
  -Wl,--export=app_init \
  -Wl,--export=app_update \
  -Wl,--export=app_draw \
  -Wl,--initial-memory=65536 \
  -Wl,--max-memory=262144 \
  -I path/to/pixelitl-sdk \
  -o myface.wasm myface.c
```

**Rust**:
```fish
# In Cargo.toml: [lib] crate-type = ["cdylib"]
cargo build --target wasm32-unknown-unknown --release
# Output: target/wasm32-unknown-unknown/release/myface.wasm
```

**AssemblyScript**:
```fish
asc myface.ts --outFile myface.wasm --optimize --runtime stub
```

**Zig**:
```fish
zig build-lib -target wasm32-freestanding myface.zig
```

Typical face binary size: **200 bytes — 8KB**. A plasma effect is ~500 bytes. A clock with full text rendering is ~2KB. A game is ~4-8KB.

---

## Budget Enforcement

### Strategy

WASM3 is a pure threaded interpreter. It doesn't support fuel metering or preemption natively. Our approach:

1. **Timer-based checking in host functions** — every host API call checks `esp_timer_get_time()`. If over budget, all subsequent calls become no-ops.
2. **Coarse but effective** — pixel art faces are draw-call heavy. A typical frame makes 200-2000 host calls. Budget checks fire every few hundred WASM instructions.
3. **Graceful degradation** — over-budget frames are skipped (display shows last good frame), not crashed.

### Budget Allocation

```
Total frame time:     33.3ms (30 FPS target)
├── WASM budget:      20.0ms (configurable per face)
├── Scale + glow:     10.0ms
├── DMA:               2.0ms (overlapped)
└── Overhead:          1.3ms
```

### Adaptive Throttling

```
State Machine:

  RUNNING ──(over budget 1x)──→ WARNED
     ↑                              │
     │                      (back on budget 5x)
  (on budget 5x)                    │
     │                              ↓
  THROTTLED ←──(over budget 10x)── WARNED
     │
     │ budget reduced by 25%
     │ min budget: 5ms
     │ recovery: +0.1ms per good frame
     │
  (over budget 100x)
     │
     ↓
  KILLED → face unloaded, error shown
```

### Runaway Loop Protection

For the edge case where a face runs a tight compute loop without calling any host functions:

```c
// In wasm_runtime.c, we can optionally instrument the wasm3 interpreter
// by patching its main dispatch loop to check a volatile flag every N ops.
// This adds ~5% overhead but catches infinite loops within 1ms.

// Alternative: use a FreeRTOS watchdog on Core 1 with a 25ms timeout.
// If the WASM task doesn't reset the watchdog, the face is force-killed.
```

Recommended approach: **FreeRTOS watchdog**. Simpler, no interpreter modification needed.

---

## Face Loading

### Sources

| Source | Method | Use Case |
|---|---|---|
| Embedded in firmware | `EMBED_FILES` in CMake | Built-in default faces |
| SPIFFS / LittleFS | Read from flash partition | User-installed faces |
| SD card | Mount + read | Offline face library |
| HTTP | WiFi download to RAM | OTA face updates, face store |

### Embedded Faces

```cmake
# In main/CMakeLists.txt
target_add_binary_data(${COMPONENT_TARGET} "faces/clock.wasm" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "faces/glucose.wasm" BINARY)
```

```c
// In main.c — access embedded WASM binaries
extern const uint8_t clock_wasm_start[] asm("_binary_clock_wasm_start");
extern const uint8_t clock_wasm_end[]   asm("_binary_clock_wasm_end");

wasm_load(&rt, "clock", clock_wasm_start, clock_wasm_end - clock_wasm_start);
```

### Filesystem Faces

```c
// Scan SPIFFS for .wasm files
DIR *dir = opendir("/spiffs/faces");
struct dirent *entry;
while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".wasm")) {
        char path[64];
        snprintf(path, sizeof(path), "/spiffs/faces/%s", entry->d_name);
        wasm_load_file(&rt, entry->d_name, path);
    }
}
```

### HTTP Loading

```c
// Download a face from a URL
// Called from Core 1 WiFi task, not from the render loop
esp_http_client_config_t config = { .url = url };
esp_http_client_handle_t client = esp_http_client_init(&config);
esp_http_client_perform(client);

int len = esp_http_client_get_content_length(client);
uint8_t *buf = malloc(len);
esp_http_client_read(client, buf, len);
esp_http_client_cleanup(client);

wasm_load(&rt, name, buf, len);
free(buf);
```

### Hot Swap

When loading a new version of an already-loaded face:

1. Create new WASM3 runtime + module
2. Call `app_init()` on the new instance
3. Run one test frame (app_update + app_draw) to verify it doesn't crash
4. Atomic swap: point the face slot to the new instance
5. Destroy old instance (free runtime + module)
6. Display never flickers — worst case shows one stale frame during swap

---

## Face Registry

The runtime maintains a registry of all loaded faces:

```c
typedef struct {
    char name[32];
    char version[16];
    
    // Source tracking
    enum { SRC_EMBEDDED, SRC_FILESYSTEM, SRC_HTTP } source;
    char source_path[128];      // "/spiffs/clock.wasm" or "https://..."
    uint32_t wasm_size;         // Binary size in bytes
    uint32_t wasm_crc32;        // For update detection
    
    // Runtime state
    wasm_app_state_t state;     // IDLE, RUNNING, WARNED, THROTTLED, KILLED
    void *wasm3_runtime;
    void *wasm3_module;
    void *fn_init, *fn_update, *fn_draw, *fn_on_gesture, *fn_destroy;
    
    // Framebuffers (double-buffered)
    uint16_t fb_front[80 * 48];
    uint16_t fb_back[80 * 48];
    
    // Budget tracking
    int64_t budget_us;          // Current budget (adaptive)
    int64_t last_frame_us;      // Last frame execution time
    int64_t avg_frame_us;       // Exponential moving average
    int overbudget_streak;
    
    // Lifetime stats
    uint32_t total_frames;
    uint32_t skipped_frames;
    uint32_t error_count;
    int64_t total_exec_us;
} face_slot_t;
```

---

## Simulator Architecture

The browser simulator serves two purposes:
1. **Preview faces** — load `.wasm` files and see exactly how they'll look on hardware
2. **Develop faces** — edit code, compile, drag-drop `.wasm`, see result instantly

### Simulator ↔ ESP32 Parity

The simulator implements the **exact same host API** as the ESP32 runtime. A `.wasm` binary runs unmodified on both platforms.

| Host Function | ESP32 Implementation | Simulator Implementation |
|---|---|---|
| `clear` | `memset(virt_fb_back, ...)` | `vfb.fill(color)` |
| `set_pixel` | Bounds-check + write to `uint16_t[]` | Bounds-check + write to `Uint32Array` |
| `draw_text` | Read from WASM linear memory, blit 5×7 font | Same, using `memory.buffer` |
| `sinf` | Native ARM `sinf()` | `Math.sin()` |
| `hsv` | Native C conversion | JS conversion (same algorithm) |
| `millis` | `esp_timer_get_time() / 1000` | `performance.now()` |
| `get_gesture` | Read from gesture mailbox | Read from mouse/touch handler |
| `storage_get` | NVS flash | `localStorage` |
| `http_get` | `esp_http_client` | `fetch()` via CORS proxy |
| Scale + glow | IRAM-cached C functions | Canvas `putImageData()` + JS kernel |

### Simulator Components

```
pixelitl-simulator.html (single file, no build step)
│
├── WASM Loader
│   ├── Drag-and-drop .wasm file onto the display
│   ├── File picker button
│   ├── URL input (load .wasm from HTTP)
│   └── Built-in demo faces (embedded as base64)
│
├── Host API (JavaScript)
│   ├── Graphics functions → write to Uint32Array(80×48)
│   ├── Color functions → RGB565 ↔ RGBA conversion
│   ├── Math functions → Math.sin, Math.cos, etc.
│   ├── Gesture handler → mouse/touch → gesture state machine
│   └── Storage → localStorage with face-name prefix
│
├── Render Pipeline
│   ├── Virtual framebuffer (Uint32Array, 80×48)
│   ├── Scale-up modes: flat, grid, dots
│   ├── Glow modes: soft, bloom, neon (pre-computed kernels)
│   └── Canvas putImageData (800×480)
│
├── UI Controls
│   ├── Face selector (tabs for loaded faces)
│   ├── Glow mode toggle
│   ├── Scale mode toggle
│   ├── Virtual resolution selector
│   ├── FPS counter + frame time graph
│   ├── WASM memory inspector
│   └── Budget simulation (artificially throttle to ESP32 speed)
│
└── Debug Panel
    ├── Console output (log_print from WASM)
    ├── Host API call counter per frame
    ├── Memory usage (WASM linear memory)
    └── Frame time breakdown (update vs draw vs render)
```

### WASM Loading in Browser

```javascript
async function loadFace(name, wasmBytes) {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 4 });
    
    const importObject = {
        env: {
            memory,
            clear:       (color) => { vfb.fill(toRGBA(color)); },
            set_pixel:   (x, y, color) => { if (inBounds(x,y)) vfb[y*80+x] = toRGBA(color); },
            fill_rect:   (x, y, w, h, color) => { /* ... */ },
            draw_text:   (x, y, ptr, len, color) => {
                const bytes = new Uint8Array(memory.buffer, ptr, len);
                const str = new TextDecoder().decode(bytes);
                return drawFont(x, y, str, color);
            },
            rgb:         (r, g, b) => toRGB565(r, g, b),
            hsv:         (h, s, v) => hsvToRGB565(h, s, v),
            sinf:        Math.sin,
            cosf:        Math.cos,
            sqrtf:       Math.sqrt,
            millis:      () => BigInt(Math.floor(performance.now())),
            width:       () => 80,
            height:      () => 48,
            get_gesture: () => consumeGesture(),
            get_touch_x: () => lastTouchX,
            get_touch_y: () => lastTouchY,
            // ...
        }
    };
    
    const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
    
    instance.exports.app_init();
    
    faces[name] = {
        instance,
        update: instance.exports.app_update,
        draw:   instance.exports.app_draw,
    };
}
```

### Simulator Frame Loop

```javascript
function loop(timestamp) {
    const dt = timestamp - lastTime;
    lastTime = timestamp;
    
    // Run active face
    const face = faces[activeFace];
    face.update(frameCount, Math.floor(dt));
    vfb.fill(0);  // clear
    face.draw();
    
    // Scale + glow
    renderToCanvas(vfb, glowMode);
    
    // Stats
    frameCount++;
    updateFPSCounter();
    
    requestAnimationFrame(loop);
}
```

### Budget Simulation Mode

The simulator can artificially slow down WASM execution to match ESP32 interpreter speed, giving accurate performance feedback:

```javascript
function runWithBudget(face, budgetMs) {
    const start = performance.now();
    
    face.update(frame, dt);
    vfb.fill(0);
    face.draw();
    
    const elapsed = performance.now() - start;
    const simulated = elapsed * INTERPRETER_SLOWDOWN_FACTOR; // ~20-50×
    
    if (simulated > budgetMs) {
        showWarning(`Over budget: ~${simulated.toFixed(1)}ms on ESP32 (limit: ${budgetMs}ms)`);
    }
    
    perfGraph.push({ native: elapsed, simulated });
}
```

The `INTERPRETER_SLOWDOWN_FACTOR` is calibrated by running a benchmark face on both the ESP32 and in the browser and measuring the ratio.

---

## File Layout

### ESP32 Project

```
pixelitl-display/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv                  ← custom partition table (add SPIFFS)
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                     ← entry point, LCD init, render loop
│   ├── panels.h / panels.c        ← drawing primitives, font, panel chrome
│   ├── gt911.h / gt911.c          ← touch driver
│   ├── glow.h                     ← bloom renderer
│   ├── wasm_runtime.h             ← WASM runtime API
│   ├── wasm_runtime.c             ← WASM3 integration, host functions, budget
│   ├── face_loader.h              ← filesystem/HTTP face loading
│   ├── face_loader.c
│   ├── wifi_manager.h / .c        ← WiFi connect + face download
│   └── faces/                     ← embedded .wasm binaries
│       ├── clock.wasm
│       ├── glucose.wasm
│       └── weather.wasm
├── components/
│   └── wasm3/                     ← git clone of wasm3
└── spiffs/                         ← filesystem image (user faces)
    └── faces/
        └── (user .wasm files)
```

### WASM SDK

```
pixelitl-sdk/
├── pixelitl.h                      ← C SDK header (imports + constants)
├── pixelitl_rust/                  ← Rust crate (optional)
│   ├── Cargo.toml
│   └── src/lib.rs
├── examples/
│   ├── Makefile
│   ├── hello.c                     ← minimal face (text + rectangle)
│   ├── clock.c                     ← clock with buttons
│   ├── plasma.c                    ← rainbow plasma effect
│   ├── gameoflife.c                ← Conway's Game of Life
│   ├── glucose.c                   ← CGM monitor with graph
│   └── pong.c                      ← touch-controlled pong game
└── tools/
    ├── build.fish                  ← compile .c → .wasm
    └── upload.fish                 ← push .wasm to device over WiFi
```

### Simulator

```
pixelitl-simulator.html              ← single self-contained file
```

---

## Flash Partition Layout

```
# partitions.csv
# Name,     Type, SubType, Offset,   Size
nvs,        data, nvs,     0x9000,   0x6000
phy_init,   data, phy,     0xf000,   0x1000
factory,    app,  factory, 0x10000,  0x200000    # 2MB firmware
spiffs,     data, spiffs,  0x210000, 0x1F0000    # ~2MB face storage
```

2MB for SPIFFS gives room for 200+ typical faces (most are 2-8KB).

---

## Network API (Face Store)

Optional: a simple HTTP API for discovering and downloading faces.

### Device → Server

```
GET /api/faces
  → [{ "name": "clock", "version": "1.2", "size": 2048, "url": "..." }, ...]

GET /api/faces/clock/latest.wasm
  → raw .wasm binary

GET /api/faces/clock/meta.json
  → { "name": "clock", "author": "...", "description": "...", "version": "1.2" }
```

### Device Sync Flow

```
1. On boot (or periodic timer):
   - GET /api/faces → list of available faces
   - Compare CRC32 of local faces vs server versions
   - Download any new/updated faces
   - Load into runtime

2. On user request (via web config page or physical button):
   - Browse face store
   - Download selected face
   - Hot-swap into runtime
```

---

## Implementation Phases

### Phase 1: Core WASM Runtime (Week 1)

- [ ] Integrate wasm3 as ESP-IDF component
- [ ] Implement `wasm_runtime.c` with real wasm3 calls (remove stubs)
- [ ] Host API: `clear`, `set_pixel`, `fill_rect`, `draw_text`, `rgb`, `hsv`, `sinf`, `cosf`
- [ ] Host API: `millis`, `width`, `height`, `get_gesture`, `get_touch_x/y`
- [ ] Budget enforcement (timer-based check in host functions)
- [ ] Double-buffered framebuffers with atomic swap
- [ ] Load embedded `.wasm` faces from flash
- [ ] Wire into existing render loop (replace native panels with WASM faces)
- [ ] Test: plasma.wasm and clock.wasm running on hardware

### Phase 2: Simulator (Week 1-2)

- [ ] HTML simulator with WASM loading via `WebAssembly.instantiate()`
- [ ] All host functions implemented in JavaScript
- [ ] Drag-and-drop `.wasm` loading
- [ ] Glow modes (reuse existing kernel code from current simulator)
- [ ] Gesture detection (mouse drag → swipe, click → tap)
- [ ] Face tab switcher
- [ ] FPS counter + frame time graph
- [ ] Console output panel

### Phase 3: Face SDK + Examples (Week 2)

- [ ] Finalize `pixelitl.h` C SDK
- [ ] Rust SDK crate
- [ ] Build system (Makefile + build.fish)
- [ ] Example faces: hello, clock, plasma, gameoflife, glucose, pong
- [ ] Size-optimize examples (strip debug info, optimize for size)
- [ ] SDK documentation

### Phase 4: Face Loading + Storage (Week 2-3)

- [ ] SPIFFS partition for user faces
- [ ] Scan + auto-load .wasm files from SPIFFS on boot
- [ ] `storage_get` / `storage_set` via NVS (ESP32) and localStorage (simulator)
- [ ] Hot-swap: load new face version without restart
- [ ] Face unloading + memory cleanup

### Phase 5: WiFi + HTTP (Week 3-4)

- [ ] WiFi manager (connect to configured AP)
- [ ] NTP time sync (for clock faces)
- [ ] `http_get` host function (with timeout + size limit)
- [ ] Face download from HTTP URL
- [ ] Simple web config page (served from ESP32) for WiFi setup + face management
- [ ] Upload .wasm via web interface

### Phase 6: Polish (Week 4)

- [ ] Watchdog for runaway WASM loops
- [ ] Face error screen (show error message when face crashes)
- [ ] System face (settings, WiFi status, face list) — always available, native C
- [ ] Transition animations between faces (reuse existing slide code)
- [ ] Budget simulation mode in simulator
- [ ] WASM memory inspector in simulator
- [ ] Performance profiling and optimization pass

---

## Open Questions

1. **WAMR vs WASM3** — Start with WASM3 for simplicity. Migrate to WAMR AOT only if we hit interpreter speed ceiling on complex faces. The host API layer abstracts the runtime, so swapping is straightforward.

2. **Rust SDK ergonomics** — Should we provide a `#[pixelitl::face]` proc macro that auto-generates the exports? Or keep it manual with `#[no_mangle] pub extern "C" fn app_draw()`?

3. **Face-to-face communication** — Should faces be able to share data? (e.g., glucose face exposes current reading, clock face shows it). Current design: no. Each face is fully isolated. Shared state can go through storage keys.

4. **Multi-face compositor** — Could we render two faces side-by-side (split screen)? The virtual resolution (80×48) makes this tight, but 40×48 per face is still usable. Worth considering for later.

5. **Simulator offline mode** — Ship demo faces as base64-encoded strings inside the HTML so the simulator works without any `.wasm` files. Then users can drag-drop their own.
