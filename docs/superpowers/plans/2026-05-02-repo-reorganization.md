# Pixelitl Display Repo Reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure the repo so the firmware lives in `firmware/`, applications live in `apps/<name>/{face,peer,service}/`, showcase demos live in `demo/<name>/`, and the firmware embeds faces via a manifest file instead of hard-coded extern blocks.

**Architecture:** This is a structural refactor. No SDK API changes, no host-function changes, no face-protocol changes. Migration is done in small, individually-buildable steps using `git mv` to preserve history. Each step ends with a build verification before moving on. The risky step (manifest-driven face embedding) is isolated to a single task and validated by a successful flash + boot.

**Tech Stack:** ESP-IDF v6.1-dev, CMake, GNU Make, clang `--target=wasm32`, wasm3.

**Spec:** [`docs/superpowers/specs/2026-05-02-repo-reorganization-design.md`](../specs/2026-05-02-repo-reorganization-design.md)

---

## Task 0: Pre-flight — untrack build artifacts

**Problem:** `build/`, `peer/build/`, `studio/_build/`, `service/_build/`, `.dexter.db*`, `sdkconfig.old` are tracked in git (1500+ files in `build/` alone). If we `git mv` them they'll bloat every subsequent commit. Untrack them first.

**Files:**
- Modify: `.gitignore`

- [ ] **Step 1: Inspect what's tracked that shouldn't be**

```bash
git ls-files build/ peer/build/ studio/_build/ service/_build/ .dexter.db* sdkconfig.old 2>/dev/null | wc -l
```
Expected: a number around 1500+. Confirms these are tracked.

- [ ] **Step 2: Replace `.gitignore` with full ignore set**

Read the current `.gitignore` first to preserve any meaningful entries:
```bash
cat .gitignore
```

Overwrite `.gitignore` with:
```gitignore
# Build outputs
build/
firmware/build/
apps/*/peer/build/
apps/*/service/_build/
peer/build/
service/_build/
studio/_build/

# Compiled face binaries (built artifacts)
firmware/main/faces/*.wasm
main/faces/*.wasm
apps/*/face/*.wasm
demo/*/*.wasm
sdk/examples/*.wasm

# IDE / editor caches
.vscode/.cache/
.cache/
**/.cache/

# Stale configs
sdkconfig.old

# Local DBs / caches
.dexter.db*
*.db-shm
*.db-wal

# macOS
.DS_Store

# Python venv
.venv/
**/.venv/

# Node
node_modules/

# Logs
*.log
```

- [ ] **Step 3: Untrack the files (preserve them on disk)**

```bash
git rm -r --cached build/ peer/build/ studio/_build/ service/_build/ 2>/dev/null
git rm --cached .dexter.db .dexter.db-shm .dexter.db-wal sdkconfig.old 2>/dev/null
git rm -r --cached main/faces/ 2>/dev/null
git rm --cached sdk/examples/*.wasm 2>/dev/null
```

- [ ] **Step 4: Verify clean diff**

```bash
git status --short | head -20
```
Expected: a long list of `D` (deletion) entries for build artifacts plus `M .gitignore`. Working tree should still have the files (only the index removed them).

- [ ] **Step 5: Commit**

```bash
git add .gitignore
git commit -m "chore: untrack build artifacts and local caches

build/, peer/build/, studio/_build/, service/_build/, .dexter.db*,
sdkconfig.old, and committed .wasm binaries should not be in source
control. Untrack them and add a comprehensive .gitignore.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 6: Verify firmware still builds**

```bash
source ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tail -10
```
Expected: `Project build complete.` Build dir is rebuilt locally but no longer tracked.

---

## Task 1: Move firmware to `firmware/`

**Problem:** The firmware (IDF project) currently squats on the repo root. Move it into `firmware/` so the root becomes a clean index of subprojects.

**Files:**
- Move: `CMakeLists.txt`, `partitions.csv`, `sdkconfig`, `sdkconfig.defaults`, `dependencies.lock`, `components/`, `managed_components/`, `main/` → `firmware/`

- [ ] **Step 1: Create `firmware/` and move IDF files**

```bash
mkdir -p firmware
git mv CMakeLists.txt firmware/
git mv partitions.csv firmware/
git mv sdkconfig firmware/
git mv sdkconfig.defaults firmware/
git mv dependencies.lock firmware/
git mv components firmware/
git mv managed_components firmware/
git mv main firmware/
```

- [ ] **Step 2: Move untracked IDF artifacts into firmware/ as well**

```bash
mv build firmware/build 2>/dev/null
```
(`build/` is now untracked but still on disk; just keeping it next to its project.)

- [ ] **Step 3: Verify the move**

```bash
ls firmware/
```
Expected:
```
CMakeLists.txt   build/       components/   dependencies.lock   main/   managed_components/   partitions.csv   sdkconfig   sdkconfig.defaults
```

- [ ] **Step 4: Verify build from new location**

```bash
cd firmware && source ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tail -10
```
Expected: `Project build complete.`

- [ ] **Step 5: Commit**

```bash
cd .. && git add -A
git commit -m "refactor: move firmware/IDF project into firmware/

The repo root now treats the firmware as one of several peer
subprojects (apps/, demo/, sdk/, studio/, tools/) rather than the
default project. Build commands now run as 'cd firmware && idf.py ...'.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Subdivide `firmware/main/` into `engine/` and `drivers/`

**Problem:** All firmware source files live flat in `firmware/main/`. Split into `engine/` (WASM platform: runtime, host, framebuffer, graphics, SIMD, font) and `drivers/` (LCD, touch, WiFi, HTTP, KV, ESP-NOW). `main.c` and `board.h` stay at the `main/` root.

**Files:**
- Create: `firmware/main/engine/`, `firmware/main/drivers/`
- Move: 7 engine files, 6 driver pairs
- Modify: `firmware/main/CMakeLists.txt`

- [ ] **Step 1: Create subdirs and move engine files**

```bash
mkdir -p firmware/main/engine firmware/main/drivers
cd firmware/main
git mv wasm_runtime.c wasm_runtime.h engine/
git mv wasm_host.c wasm_host.h engine/
git mv framebuffer.c framebuffer.h engine/
git mv graphics.c graphics.h engine/
git mv simd_ops.S simd_ops.h engine/
git mv font5x7.h engine/
cd ../..
```

- [ ] **Step 2: Move driver files**

```bash
cd firmware/main
git mv lcd.c lcd.h drivers/
git mv touch.c touch.h drivers/
git mv wifi.c wifi.h drivers/
git mv http_request.c http_request.h drivers/
git mv kv_store.c kv_store.h drivers/
git mv esp_now_recv.c esp_now_recv.h drivers/
cd ../..
```

- [ ] **Step 3: Verify what's left at `main/` root**

```bash
ls firmware/main/
```
Expected:
```
CMakeLists.txt   Kconfig.projbuild   board.h   drivers/   engine/   faces/   idf_component.yml   main.c
```

- [ ] **Step 4: Update `firmware/main/CMakeLists.txt` to point at new paths**

Replace the entire file with:
```cmake
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
```

- [ ] **Step 5: Verify build**

```bash
cd firmware && idf.py build 2>&1 | tail -15
```
Expected: `Project build complete.`

If any `#include "foo.h"` fails, it means a file used to be in `main/` and now isn't — but `INCLUDE_DIRS "." "engine" "drivers"` should cover all of them. If a build error mentions a missing header, find which subdir it's in and confirm that subdir is in `INCLUDE_DIRS`.

- [ ] **Step 6: Commit**

```bash
cd .. && git add -A
git commit -m "refactor: split firmware/main into engine/ and drivers/

engine/ holds the WASM execution platform (runtime, host functions,
framebuffer, graphics, SIMD, font). drivers/ holds peripheral and
network code (LCD, touch, WiFi, HTTP, KV store, ESP-NOW). main.c
and board.h stay at the component root.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Create `sdk/face.mk` (shared wasm32 build rule)

**Files:**
- Create: `sdk/face.mk`, `sdk/README.md`

- [ ] **Step 1: Write `sdk/face.mk`**

```makefile
# Shared wasm32 build rule for pixelitl faces.
#
# Usage in app/demo Makefile:
#   NAME = dashboard
#   SOURCES = dashboard.c                # optional, defaults to $(NAME).c
#   CFLAGS_EXTRA = -I.                   # optional
#   include ../../../sdk/face.mk         # apps (depth 3)
# or for demos at depth 2:
#   include ../../sdk/face.mk

CC ?= clang
SOURCES ?= $(NAME).c
SDK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
REPO_ROOT := $(SDK_DIR)..

CFLAGS = --target=wasm32 -O2 -nostdlib -fno-builtin \
    -Wl,--no-entry \
    -Wl,--export=app_init \
    -Wl,--export=app_update \
    -Wl,--export=app_draw \
    -Wl,--allow-undefined \
    -Wl,--initial-memory=131072 \
    -Wl,--max-memory=262144 \
    -I$(SDK_DIR) $(CFLAGS_EXTRA)

FIRMWARE_FACES_DIR := $(REPO_ROOT)/firmware/main/faces

.PHONY: all install clean

all: $(NAME).wasm

$(NAME).wasm: $(SOURCES) $(SDK_DIR)pixelitl.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

install: $(NAME).wasm
	mkdir -p $(FIRMWARE_FACES_DIR)
	cp $(NAME).wasm $(FIRMWARE_FACES_DIR)/

clean:
	rm -f $(NAME).wasm
```

- [ ] **Step 2: Write a minimal `sdk/README.md`**

```markdown
# Pixelitl SDK

Headers and shared build rules for writing faces that run on the
pixelitl display.

- `pixelitl.h` — C SDK (host-function imports + helper macros)
- `pixelitl.ts` — TypeScript / AssemblyScript SDK
- `face.mk` — shared wasm32 build rule for face Makefiles

## Building a face

Each face has its own one-line Makefile:

```makefile
NAME = my_face
include ../../../sdk/face.mk     # apps/<name>/face/Makefile
# or
include ../../sdk/face.mk        # demo/<name>/Makefile
```

Then:

```fish
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install
```

`install` copies `<name>.wasm` into `firmware/main/faces/` so the
firmware build embeds it.
```

- [ ] **Step 3: Smoke-test the build rule against an existing face**

```bash
cd sdk/examples
PATH="/opt/homebrew/opt/llvm/bin:$PATH" \
  make -f - <<'EOF'
NAME = plasma
SOURCES = plasma.c
include ../face.mk
EOF
ls plasma.wasm
```
Expected: `plasma.wasm` exists. (Won't install since FIRMWARE_FACES_DIR is wrong from this depth, but the compile rule works.)

```bash
rm plasma.wasm
cd ../..
```

- [ ] **Step 4: Commit**

```bash
git add sdk/face.mk sdk/README.md
git commit -m "feat(sdk): add shared face.mk wasm32 build rule

Each app/demo Makefile becomes a 2-line config that includes face.mk
for the actual clang invocation, exports, memory limits, and install
target. Replaces the per-Makefile duplication of build flags.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Move `weather` app (smallest face-only app, validates the apps/ pattern)

**Why first:** `weather` is small, has no peer/service, and isn't currently embedded — if anything goes wrong with the per-app Makefile pattern, the firmware build is unaffected.

**Files:**
- Create: `apps/weather/face/Makefile`
- Move: `sdk/examples/weather.c` → `apps/weather/face/weather.c`

- [ ] **Step 1: Create the dir and move source**

```bash
mkdir -p apps/weather/face
git mv sdk/examples/weather.c apps/weather/face/
```

- [ ] **Step 2: Create `apps/weather/face/Makefile`**

```makefile
NAME = weather
include ../../../sdk/face.mk
```

- [ ] **Step 3: Build and install**

```bash
cd apps/weather/face
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install
ls ../../../firmware/main/faces/weather.wasm
cd ../../..
```
Expected: `firmware/main/faces/weather.wasm` exists.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "refactor: move weather face to apps/weather/face/

Validates the per-app Makefile + sdk/face.mk pattern before moving
larger apps. weather is not embedded into firmware so this move is
zero-risk for the device build.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Move `glucose` and `store` apps (face-only)

**Files:**
- Create: `apps/glucose/face/Makefile`, `apps/store/face/Makefile`
- Move: `sdk/examples/glucose.c` → `apps/glucose/face/glucose.c`, `sdk/examples/store.c` → `apps/store/face/store.c`

- [ ] **Step 1: Move glucose**

```bash
mkdir -p apps/glucose/face
git mv sdk/examples/glucose.c apps/glucose/face/
```

- [ ] **Step 2: Create `apps/glucose/face/Makefile`**

```makefile
NAME = glucose
include ../../../sdk/face.mk
```

- [ ] **Step 3: Move store**

```bash
mkdir -p apps/store/face
git mv sdk/examples/store.c apps/store/face/
```

- [ ] **Step 4: Create `apps/store/face/Makefile`**

```makefile
NAME = store
include ../../../sdk/face.mk
```

- [ ] **Step 5: Build both**

```bash
cd apps/glucose/face && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install && cd ../../..
cd apps/store/face   && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install && cd ../../..
ls firmware/main/faces/glucose.wasm firmware/main/faces/store.wasm
```
Expected: both `.wasm` files exist.

- [ ] **Step 6: Verify firmware still builds (uses the freshly-installed binaries)**

```bash
cd firmware && idf.py build 2>&1 | tail -5 && cd ..
```
Expected: `Project build complete.`

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: move glucose and store faces to apps/

Both are face-only apps (no peer/service). Firmware build still
embeds them from firmware/main/faces/ as before.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Move `dashboard` app (face + service)

**Files:**
- Create: `apps/dashboard/face/Makefile`, `apps/dashboard/face/dota_sprite.h` (move)
- Move: `sdk/examples/dashboard.c` → `apps/dashboard/face/dashboard.c`
- Move: `sdk/examples/dota_sprite.h` → `apps/dashboard/face/dota_sprite.h`
- Move: `service/` → `apps/dashboard/service/`

- [ ] **Step 1: Move face source**

```bash
mkdir -p apps/dashboard/face
git mv sdk/examples/dashboard.c apps/dashboard/face/
git mv sdk/examples/dota_sprite.h apps/dashboard/face/
```

- [ ] **Step 2: Create `apps/dashboard/face/Makefile`**

(`dashboard.c` `#include "dota_sprite.h"` so we need `-I.` for the local header.)

```makefile
NAME = dashboard
CFLAGS_EXTRA = -I.
include ../../../sdk/face.mk
```

- [ ] **Step 3: Move the service**

```bash
git mv service apps/dashboard/service
mv service/_build apps/dashboard/service/_build 2>/dev/null
```

- [ ] **Step 4: Build the face**

```bash
cd apps/dashboard/face && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install && cd ../../..
ls firmware/main/faces/dashboard.wasm
```
Expected: `dashboard.wasm` updated.

- [ ] **Step 5: Smoke-test the service still compiles in its new home**

```bash
cd apps/dashboard/service && mix compile 2>&1 | tail -5 && cd ../../..
```
Expected: compiles cleanly (no path errors). If `mix.exs` references files relative to the project, they should still resolve since the whole tree moved together.

- [ ] **Step 6: Verify firmware still builds**

```bash
cd firmware && idf.py build 2>&1 | tail -5 && cd ..
```
Expected: `Project build complete.`

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: bundle dashboard face and service under apps/dashboard/

The Phoenix service (HN feed, sprite generation, DOTA alert) is only
consumed by the dashboard face. Bundling them makes the dependency
explicit from the layout.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Move `radar` app (face + peer)

**Files:**
- Create: `apps/radar/face/Makefile`
- Move: `sdk/examples/radar.c` → `apps/radar/face/radar.c`
- Move: `peer/` → `apps/radar/peer/`

- [ ] **Step 1: Move face source**

```bash
mkdir -p apps/radar/face
git mv sdk/examples/radar.c apps/radar/face/
```

- [ ] **Step 2: Create `apps/radar/face/Makefile`**

```makefile
NAME = radar
include ../../../sdk/face.mk
```

- [ ] **Step 3: Move the peer**

```bash
git mv peer apps/radar/peer
mv peer/build apps/radar/peer/build 2>/dev/null
```

- [ ] **Step 4: Build the face**

```bash
cd apps/radar/face && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make install && cd ../../..
ls firmware/main/faces/radar.wasm
```
Expected: `radar.wasm` updated.

- [ ] **Step 5: Smoke-test the peer still builds**

```bash
cd apps/radar/peer && idf.py set-target esp32 2>&1 | tail -3 && idf.py build 2>&1 | tail -5 && cd ../../..
```
Expected: `Project build complete.` (peer is its own IDF project, builds independently of the display firmware).

If `set-target esp32` fails because target is already set, that's fine — skip it and just run `idf.py build`.

- [ ] **Step 6: Verify display firmware still builds**

```bash
cd firmware && idf.py build 2>&1 | tail -5 && cd ..
```
Expected: `Project build complete.`

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: bundle radar face and peer under apps/radar/

The ESP32-WROOM peer (with LD2450 mmWave radar) only exists to feed
the radar face via ESP-NOW. Bundling them makes the dependency
explicit from the layout.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Move all demos to `demo/<name>/`

**Files:**
- Create: 8 demo dirs each with a Makefile
- Move: 8 `.c` source files

- [ ] **Step 1: Move each demo source to its own dir**

```bash
for name in plasma clock nyan holi buttons invaders hd_demo ultra_demo; do
  mkdir -p demo/$name
  git mv sdk/examples/$name.c demo/$name/
done
```

- [ ] **Step 2: Create a Makefile for each demo**

For each of the 8 demos, create `demo/<name>/Makefile` with:
```makefile
NAME = <name>
include ../../sdk/face.mk
```

Use this loop to generate them:
```bash
for name in plasma clock nyan holi buttons invaders hd_demo ultra_demo; do
  cat > demo/$name/Makefile <<EOF
NAME = $name
include ../../sdk/face.mk
EOF
done
```

- [ ] **Step 3: Verify all demos build**

```bash
for name in plasma clock nyan holi buttons invaders hd_demo ultra_demo; do
  echo "=== $name ==="
  (cd demo/$name && PATH="/opt/homebrew/opt/llvm/bin:$PATH" make 2>&1 | tail -2)
done
```
Expected: each demo prints a clang invocation and produces `<name>.wasm`. No errors.

- [ ] **Step 4: Clean the .wasm outputs (they're untracked anyway)**

```bash
for name in plasma clock nyan holi buttons invaders hd_demo ultra_demo; do
  rm -f demo/$name/$name.wasm
done
```

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: move showcase demos to demo/<name>/

Each demo gets its own dir with a 2-line Makefile that includes the
shared sdk/face.mk. Demos are engine showcase toys (plasma, clock,
nyan, holi, buttons, invaders, hd_demo, ultra_demo) — distinct from
the real applications under apps/.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: Manifest-driven face embedding

**Files:**
- Create: `firmware/main/embedded_faces.txt`
- Modify: `firmware/main/CMakeLists.txt`
- Modify: `firmware/main/main.c`

**Why this is the highest-risk task:** changes to how faces are embedded into flash and how `main.c` registers them. Verified by a successful flash + boot + face-switch.

- [ ] **Step 1: Create `firmware/main/embedded_faces.txt`**

```
dashboard
glucose
store
radar
```

- [ ] **Step 2: Replace `firmware/main/CMakeLists.txt` with manifest-driven version**

```cmake
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
```

- [ ] **Step 3: Read current `firmware/main/main.c` to find the registration block**

```bash
grep -n "register_face\|wasm_start\|wasm_end" firmware/main/main.c
```
Expected: lines around 21-28 (extern declarations) and 146-157 (register_face calls + active-face fallback) need to be replaced.

- [ ] **Step 4: Replace the extern block in `firmware/main/main.c`**

Find this block (lines ~20-28):
```c
// Embedded face binaries
extern const uint8_t store_wasm_start[]     asm("_binary_store_wasm_start");
extern const uint8_t store_wasm_end[]       asm("_binary_store_wasm_end");
extern const uint8_t glucose_wasm_start[]   asm("_binary_glucose_wasm_start");
extern const uint8_t glucose_wasm_end[]     asm("_binary_glucose_wasm_end");
extern const uint8_t dashboard_wasm_start[] asm("_binary_dashboard_wasm_start");
extern const uint8_t dashboard_wasm_end[]   asm("_binary_dashboard_wasm_end");
extern const uint8_t radar_wasm_start[]     asm("_binary_radar_wasm_start");
extern const uint8_t radar_wasm_end[]       asm("_binary_radar_wasm_end");
```

Replace with:
```c
// Embedded face binaries — generated from embedded_faces.txt at build time.
#include "embedded_faces.h"
```

- [ ] **Step 5: Replace the registration block in `firmware/main/main.c`**

Find this block (lines ~146-157):
```c
    // Register all embedded faces — m3 runtimes are NOT created here. Each
    // face is brought up only when the user switches to it (and the previous
    // face's runtime is freed). This keeps internal RAM uncontended for WiFi
    // and other shared infrastructure.
    int dashboard_id = wasm_register_face("dashboard",
        dashboard_wasm_start, dashboard_wasm_end - dashboard_wasm_start);
    int glucose_id = wasm_register_face("glucose",
        glucose_wasm_start, glucose_wasm_end - glucose_wasm_start);
    int store_id = wasm_register_face("store",
        store_wasm_start, store_wasm_end - store_wasm_start);
    int radar_id = wasm_register_face("radar",
        radar_wasm_start, radar_wasm_end - radar_wasm_start);
    (void)radar_id;
    if (dashboard_id >= 0) s_active_face = dashboard_id;
    else if (glucose_id >= 0) s_active_face = glucose_id;
    else if (store_id >= 0) s_active_face = store_id;
```

Replace with:
```c
    // Register all embedded faces from the manifest. m3 runtimes are NOT
    // created here — each face is brought up only when the user switches
    // to it (and the previous face's runtime is freed). This keeps internal
    // RAM uncontended for WiFi and other shared infrastructure.
    int first_id = -1;
    for (int i = 0; i < EMBEDDED_FACE_COUNT; i++) {
        const struct embedded_face_s *ef = &EMBEDDED_FACES[i];
        int id = wasm_register_face(ef->name, ef->start, ef->end - ef->start);
        if (id >= 0 && first_id < 0) first_id = id;
    }
    if (first_id >= 0) s_active_face = first_id;
```

- [ ] **Step 6: Build the firmware**

```bash
cd firmware && idf.py build 2>&1 | tail -15
```
Expected: `Project build complete.` If you see `embedded_faces.h: No such file or directory`, the generated header isn't being made or the include dir isn't right — verify `${CMAKE_CURRENT_BINARY_DIR}` is in `INCLUDE_DIRS` and `file(GENERATE ...)` ran (the header should be at `firmware/build/esp-idf/main/embedded_faces.h`).

- [ ] **Step 7: Inspect the generated header**

```bash
cat firmware/build/esp-idf/main/embedded_faces.h
```
Expected: `#define EMBEDDED_FACE_COUNT 4`, four `extern` pairs, an `EMBEDDED_FACES[]` array with 4 entries.

- [ ] **Step 8: Flash and verify all 4 faces work**

```bash
cd firmware && idf.py flash 2>&1 | tail -5 && cd ..
```
Expected: `Hash of data verified. Hard resetting via RTS pin.`

Then connect with `idf.py monitor` (Ctrl+] to exit) and confirm:
- Boot logs show `Loaded 4 WASM face(s), active=N` (N is the dashboard's id, typically 0).
- Swipe left/right on the touchscreen — face name in logs should cycle through `dashboard`, `glucose`, `store`, `radar`.
- Each face renders correctly.

(If the user wants to skip flashing during plan execution, they can do this verification later as long as `idf.py build` succeeds.)

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "feat(firmware): manifest-driven embedded faces

firmware/main/embedded_faces.txt lists which faces ship in flash.
firmware/main/CMakeLists.txt reads it and generates embedded_faces.h
with the extern declarations and an EMBEDDED_FACES[] array. main.c
loops over the array to register them.

Adding/removing an embedded face is now a one-line edit to
embedded_faces.txt — no more per-face extern blocks or hardcoded
register_face calls in main.c.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: Delete `wasm_load_face` (legacy, post-refactor dead code)

**Files:**
- Modify: `firmware/main/engine/wasm_runtime.h`, `firmware/main/engine/wasm_runtime.c`

`wasm_load_face` is superseded by `wasm_register_face` + `wasm_face_ensure_loaded`. The dynamic-install path uses `wasm_install_commit` which can switch to the same pair.

- [ ] **Step 1: Find all callers**

```bash
grep -rn "wasm_load_face" firmware/ apps/ demo/ studio/
```
Expected: declarations in `wasm_runtime.h`, definition in `wasm_runtime.c`, possibly use in `wasm_install_commit` (inside `wasm_runtime.c`).

- [ ] **Step 2: Read `wasm_install_commit` to understand how it currently uses `wasm_load_face`**

```bash
grep -n "wasm_install_commit\|wasm_load_face" firmware/main/engine/wasm_runtime.c
```
Note the line range of `wasm_install_commit`.

- [ ] **Step 3: Read the full `wasm_install_commit` body**

```bash
awk '/^[a-zA-Z_].*wasm_install_commit/,/^}/' firmware/main/engine/wasm_runtime.c
```
Identify the call to `wasm_load_face`. The replacement: call `wasm_register_face` to register the binary, then `wasm_face_ensure_loaded(id)` to bring up its m3 runtime. Return the id (or `-1` on failure).

- [ ] **Step 4: Replace `wasm_load_face` call inside `wasm_install_commit`**

In `firmware/main/engine/wasm_runtime.c`, find the line that calls `wasm_load_face(...)` inside `wasm_install_commit` and replace it with:

```c
    int id = wasm_register_face(name, bin_ptr, bin_size);
    if (id < 0) return -1;
    if (wasm_face_ensure_loaded(id) != ESP_OK) {
        // Registration succeeded but runtime alloc failed; tear it back down.
        wasm_face_unload_runtime(id);
        return -1;
    }
    return id;
```

(Use the actual variable names from the surrounding code — `name`, `bin_ptr`, `bin_size` may be named differently. Match what `wasm_install_commit` already has.)

- [ ] **Step 5: Delete the `wasm_load_face` declaration in `wasm_runtime.h`**

Remove this block from `firmware/main/engine/wasm_runtime.h`:
```c
// Old: register + load in one shot. Still used by the dynamic install flow.
int       wasm_load_face(const char *name, const uint8_t *wasm_data, uint32_t wasm_size);
```

- [ ] **Step 6: Delete the `wasm_load_face` definition in `wasm_runtime.c`**

Find the `int wasm_load_face(...)` function definition in `firmware/main/engine/wasm_runtime.c` and delete the entire function body.

- [ ] **Step 7: Verify no callers remain**

```bash
grep -rn "wasm_load_face" firmware/ apps/ demo/ studio/
```
Expected: empty output.

- [ ] **Step 8: Build firmware**

```bash
cd firmware && idf.py build 2>&1 | tail -5 && cd ..
```
Expected: `Project build complete.`

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "refactor(engine): remove wasm_load_face

Superseded by wasm_register_face + wasm_face_ensure_loaded. The
dynamic-install flow (wasm_install_commit) now uses the same pair as
embedded faces, removing the only remaining caller.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: Delete superseded files and cruft

**Files:**
- Delete: `tools/pixelitl_service.exs`, `sdkconfig.old`, `sdk/examples/`, `sdk/examples/Makefile`

- [ ] **Step 1: Confirm `sdk/examples/` is now empty of source**

```bash
ls sdk/examples/
```
Expected: only `Makefile` and `index.txt` (and possibly `*.wasm` build outputs which are gitignored).

- [ ] **Step 2: Delete the superseded files**

```bash
git rm tools/pixelitl_service.exs
git rm -r sdk/examples
rm -f sdkconfig.old   # already untracked, remove from disk
```

- [ ] **Step 3: Verify firmware still builds**

```bash
cd firmware && idf.py build 2>&1 | tail -5 && cd ..
```
Expected: `Project build complete.`

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: remove superseded files

- tools/pixelitl_service.exs: superseded by apps/dashboard/service/
  (the same Phoenix app, but as a real project rather than a
  Mix.install single-file).
- sdk/examples/: all sources moved to apps/<name>/face/ or demo/<name>/.
- sdkconfig.old: stale 88 KB IDF config snapshot.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: Top-level `Makefile` that builds every face

**Files:**
- Create: `Makefile` (repo root)

- [ ] **Step 1: Write the root `Makefile`**

```makefile
# Top-level Makefile — builds every face under apps/ and demo/ and
# installs the embedded ones into firmware/main/faces/.
#
# Usage:
#   make           # build + install every face
#   make clean     # clean every face
#   make apps      # build + install just the apps
#   make demo      # build + install just the demos

APP_FACES  := $(wildcard apps/*/face)
DEMO_FACES := $(wildcard demo/*)

.PHONY: all apps demo clean
.PHONY: $(APP_FACES) $(DEMO_FACES)

all: apps demo

apps: $(APP_FACES)

demo: $(DEMO_FACES)

$(APP_FACES) $(DEMO_FACES):
	$(MAKE) -C $@ install

clean:
	@for d in $(APP_FACES) $(DEMO_FACES); do \
	  $(MAKE) -C $$d clean; \
	done
```

- [ ] **Step 2: Smoke-test it**

```bash
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make 2>&1 | tail -20
```
Expected: builds every face, copies the embedded ones into `firmware/main/faces/`. Verify with:

```bash
ls firmware/main/faces/
```
Expected: at least the 4 embedded faces (`dashboard.wasm`, `glucose.wasm`, `store.wasm`, `radar.wasm`) plus any other face whose Makefile also installs there (apps install; demos build but don't install since they aren't embedded — to keep the wasm binaries discoverable, the install step still copies them, harmless even if they aren't embedded).

- [ ] **Step 3: Commit**

```bash
git add Makefile
git commit -m "build: add top-level Makefile that walks apps/ and demo/

'make' from the repo root now builds every face and installs them
into firmware/main/faces/. Per-app/per-demo Makefiles each include
sdk/face.mk and only declare NAME, so the root Makefile just
recurses with the same install target.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 13: Update `README.md` and `ARCHITECTURE.md` to reflect new layout

**Files:**
- Modify: `README.md`
- Modify: `ARCHITECTURE.md`

- [ ] **Step 1: Read the current README to see what needs updating**

```bash
head -80 README.md
```

- [ ] **Step 2: Update the "Layout" / "Project structure" section**

Find the section in `README.md` that describes the directory layout. Replace it with:

```markdown
## Project layout

```
pixelitl-display/
├── firmware/         # ESP32-S3 display firmware (the engine)
│   └── main/
│       ├── engine/   # WASM runtime, framebuffer, graphics, SIMD
│       ├── drivers/  # LCD, touch, WiFi, HTTP, KV, ESP-NOW
│       └── embedded_faces.txt  # which faces ship in flash
├── sdk/              # face SDK headers + shared face.mk build rule
├── apps/             # real applications
│   ├── dashboard/    # face + Phoenix service (HN/sprite/dota)
│   ├── glucose/      # face only
│   ├── store/        # face only — talks to studio for face library
│   ├── radar/        # face + ESP32-WROOM peer with LD2450 mmWave
│   └── weather/      # face only
├── demo/             # engine showcase faces (plasma, clock, nyan, ...)
├── studio/           # Phoenix LiveView face editor + simulator
├── tools/            # rasterize_sprite.py, rasterize_stdin.py
└── docs/             # design docs and implementation plans
```

## Building

```fish
# Build all faces and install embedded ones
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make

# Build and flash the firmware
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build flash monitor
```

To add or remove an embedded face, edit `firmware/main/embedded_faces.txt`
and rebuild.
```

- [ ] **Step 3: Update build/run instructions throughout `README.md`**

Find any `idf.py build`, `cd peer && idf.py ...`, `cd service && mix ...`, or `cd sdk/examples && make ...` and update to the new paths:
- `idf.py build` → `cd firmware && idf.py build`
- `cd peer` → `cd apps/radar/peer`
- `cd service` → `cd apps/dashboard/service`
- `cd sdk/examples && make install` → `make` (from repo root)

- [ ] **Step 4: Update `ARCHITECTURE.md` similarly**

Read the file:
```bash
head -50 ARCHITECTURE.md
```

Update any directory references (look for `main/`, `sdk/examples/`, `peer/`, `service/`) to the new paths.

- [ ] **Step 5: Commit**

```bash
git add README.md ARCHITECTURE.md
git commit -m "docs: update README and ARCHITECTURE for new layout

Reflects the move of firmware to firmware/, apps to apps/<name>/,
demos to demo/<name>/, peer to apps/radar/peer/, and service to
apps/dashboard/service/.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Final verification

- [ ] **Step 1: Repo root sanity check**

```bash
ls
```
Expected (rough): `ARCHITECTURE.md  DESIGN.md  Makefile  README.md  apps  demo  docs  firmware  sdk  studio  tools  .gitignore  .git`

No more loose `CMakeLists.txt`, `sdkconfig*`, `partitions.csv`, `components/`, `managed_components/`, `main/`, `peer/`, `service/`, `build/` at the root.

- [ ] **Step 2: Full build from cold**

```bash
git clean -fdx -e .dexter.db* -e firmware/managed_components -e firmware/dependencies.lock -e .git -n | head -20
```
(`-n` is dry-run — review what would be deleted before actually doing it.)

If safe, drop `-n` to actually clean. Then:

```bash
PATH="/opt/homebrew/opt/llvm/bin:$PATH" make && \
  cd firmware && source ~/esp/esp-idf/export.sh && idf.py build
```
Expected: every face builds, firmware builds.

- [ ] **Step 3: Flash and confirm device boots**

```bash
cd firmware && idf.py flash monitor
```
Expected:
- Boot logs show `Loaded 4 WASM face(s), active=N`.
- Touch swipe left/right cycles through dashboard, glucose, store, radar.
- Each face renders correctly.
- WiFi connects.
- ESP-NOW packets arrive (if peer is also flashed).

Press Ctrl+] to exit monitor.

- [ ] **Step 4: Confirm dead code is gone**

```bash
git grep wasm_load_face
```
Expected: no matches.

```bash
ls sdk/examples 2>&1
```
Expected: `ls: sdk/examples: No such file or directory`

- [ ] **Step 5: Final commit (only if you made any cleanup changes during verification)**

If verification produced any small fixes, commit them with a clear message. Otherwise this step is a no-op.

---

## Self-review notes (for plan authors)

**Spec coverage:**
- ✅ Move firmware to `firmware/` — Task 1
- ✅ Engine + drivers split inside main/ — Task 2
- ✅ `sdk/face.mk` shared rule — Task 3
- ✅ Apps moved with bundled parts — Tasks 4, 5, 6, 7
- ✅ Demos moved — Task 8
- ✅ Manifest-driven embedded faces + main.c loop — Task 9
- ✅ Delete `wasm_load_face` — Task 10
- ✅ Delete superseded files — Task 11
- ✅ Top-level Makefile — Task 12
- ✅ README + ARCHITECTURE updates — Task 13
- ✅ `.gitignore` updates — Task 0
- ✅ Final verification — Final section
- ⚠️ `font5x7.h` move noted in spec — covered implicitly by Task 2 (`git mv font5x7.h engine/`)
- ⚠️ Studio stays at top level — no task needed; it's untouched

**Type/name consistency:**
- `EMBEDDED_FACES[]` array, `EMBEDDED_FACE_COUNT` macro, `struct embedded_face_s` — used consistently in the CMake header generation (Task 9 Step 2) and the main.c loop (Task 9 Step 5).
- `wasm_register_face`, `wasm_face_ensure_loaded`, `wasm_face_unload_runtime` — names match the existing header (`firmware/main/engine/wasm_runtime.h`).

**Risk per task:**
- Tasks 0–3: zero device risk (no firmware behavior change).
- Tasks 4–8: zero device risk if the install copies of `.wasm` files match what the firmware embeds.
- Task 9: highest risk — gated by a successful flash + boot.
- Tasks 10–13: low risk, build-only changes.
