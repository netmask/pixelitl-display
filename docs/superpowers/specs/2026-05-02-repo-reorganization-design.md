# Pixelitl Display — Repo Reorganization & Engine Cleanup

**Date:** 2026-05-02
**Status:** Approved — ready for implementation plan

## Goal

The repo grew organically. Today the firmware (the engine) sits at the repo root and shares space with subprojects (`peer/`, `service/`, `studio/`, `sdk/`, `tools/`). Faces — both throwaway demos and real applications — live flat in `sdk/examples/`. There is no clear separation between "the engine," "real apps," and "showcase demos." The goal is a layout that:

1. Treats the firmware as one peer among several subprojects (not a squatter on the root).
2. Distinguishes real applications (`apps/`) from engine showcase toys (`demo/`).
3. Bundles each app's parts together — face + peer firmware + backend service all under one app dir.
4. Cleans dead code from the engine.
5. Removes coupling between the firmware build and the catalog of apps it ships.

This is a structural refactor only. No SDK API changes, no host-function changes, no face-protocol changes.

## Target layout

```
pixelitl-display/
├── README.md
├── ARCHITECTURE.md
├── DESIGN.md
├── docs/
│   └── superpowers/specs/
│
├── firmware/                       # the engine (was repo root)
│   ├── CMakeLists.txt              # IDF top-level
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   ├── dependencies.lock
│   ├── embedded_faces.txt          # manifest: which app faces ship in flash
│   ├── components/wasm3/
│   ├── managed_components/
│   └── main/
│       ├── CMakeLists.txt          # reads embedded_faces.txt
│       ├── main.c                  # face loop replaces extern block
│       ├── board.h
│       ├── engine/
│       │   ├── wasm_runtime.{c,h}
│       │   ├── wasm_host.{c,h}
│       │   ├── framebuffer.{c,h}
│       │   ├── graphics.{c,h}
│       │   ├── simd_ops.{S,h}
│       │   └── font5x7.h
│       └── drivers/
│           ├── lcd.{c,h}
│           ├── touch.{c,h}
│           ├── wifi.{c,h}
│           ├── http_request.{c,h}
│           ├── kv_store.{c,h}
│           └── esp_now_recv.{c,h}
│
├── sdk/
│   ├── pixelitl.h
│   ├── pixelitl.ts
│   ├── face.mk                     # shared wasm32 build rule
│   └── README.md
│
├── apps/                           # real applications
│   ├── dashboard/
│   │   ├── face/
│   │   │   ├── Makefile
│   │   │   ├── dashboard.c
│   │   │   └── dota_sprite.h
│   │   └── service/                # was top-level service/
│   │       ├── lib/, priv/, config/
│   │       ├── Dockerfile, fly.toml
│   │       └── mix.exs, mix.lock
│   ├── glucose/face/
│   ├── store/face/
│   ├── radar/
│   │   ├── face/
│   │   └── peer/                   # was top-level peer/
│   │       ├── CMakeLists.txt, sdkconfig.defaults
│   │       └── main/
│   └── weather/face/
│
├── demo/                           # engine showcase (toys)
│   ├── plasma/{Makefile,plasma.c}
│   ├── clock/
│   ├── nyan/
│   ├── holi/
│   ├── buttons/
│   ├── invaders/
│   ├── hd_demo/
│   └── ultra_demo/
│
├── studio/                         # stays top-level (dev tool, not an app)
│
├── tools/
│   ├── rasterize_sprite.py         # PNG → C header
│   └── rasterize_stdin.py          # PNG stdin → JSON (also vendored in apps/dashboard/service/priv/)
│
└── Makefile                        # walks apps/ and demo/ to build everything
```

## Design decisions

### Apps vs demo

- **`apps/`** holds full applications you'd actually run on the device. Each app dir is a self-contained product. Today: `dashboard`, `glucose`, `store`, `radar`, `weather`.
- **`demo/`** holds short showcase faces that exercise engine features. Today: `plasma`, `clock`, `nyan`, `holi`, `buttons`, `invaders`, `hd_demo`, `ultra_demo`.

The redundant `sdk/examples/demo.c` (and its `.wasm`) is dropped — the rest of `demo/` covers what it was demonstrating.

### Bundle by app

Each app dir owns every component of the product:

- `apps/dashboard/face/` + `apps/dashboard/service/` (the Phoenix HN/sprite/dota backend only `dashboard` consumes).
- `apps/radar/face/` + `apps/radar/peer/` (the ESP32-WROOM with LD2450 only `radar` consumes).

Apps with only a face (`glucose`, `store`, `weather`) just contain `apps/<name>/face/`.

This makes the dependency obvious from the layout. Deleting an app cleanly deletes everything it brought.

`studio/` stays at the top level — it's a dev tool that touches every app (editor + simulator + face library API), not part of any single app.

### Firmware lives in `firmware/`

All IDF-managed files (`CMakeLists.txt`, `partitions.csv`, `sdkconfig.defaults`, `dependencies.lock`, `components/`, `managed_components/`, `main/`, `build/`) move under `firmware/`. The repo root becomes a clean index of subprojects.

Build commands change once: `cd firmware && idf.py build` instead of `idf.py build`.

### Engine vs drivers split inside `firmware/main/`

`firmware/main/` is subdivided:

- `engine/` — anything that's part of the WASM execution platform: runtime, host functions, framebuffer, graphics primitives, SIMD ops, font.
- `drivers/` — anything that talks to peripherals or the network: LCD, touch, WiFi, HTTP, KV store, ESP-NOW.

`main.c` and `board.h` stay at the `main/` root (entry point + board pin definitions).

### Embedded face manifest

`firmware/main/embedded_faces.txt` lists faces, one per line:

```
dashboard
glucose
store
radar
```

`firmware/main/CMakeLists.txt` reads this file, calls `target_add_binary_data` for each `../../../apps/<name>/face/<name>.wasm`, and configures a generated `embedded_faces.h`:

```c
#define EMBEDDED_FACE_COUNT 4

extern const uint8_t dashboard_wasm_start[] asm("_binary_dashboard_wasm_start");
extern const uint8_t dashboard_wasm_end[]   asm("_binary_dashboard_wasm_end");
/* ... one pair per face ... */

static const struct {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
} EMBEDDED_FACES[] = {
    {"dashboard", dashboard_wasm_start, dashboard_wasm_end},
    /* ... */
};
```

`main.c` loops over `EMBEDDED_FACES` to register them. Adding/removing an embedded face = edit `embedded_faces.txt`.

### Per-app Makefile + shared `sdk/face.mk`

Each app/demo Makefile is two-to-four lines:

```makefile
NAME = dashboard
include ../../../sdk/face.mk
```

`sdk/face.mk` carries the wasm32 build rule (clang `--target=wasm32`, exports, memory limits, install target that copies into `firmware/main/faces/`). Per-app overrides via `SOURCES = ...` and `CFLAGS_EXTRA = ...`.

A root `Makefile` walks `apps/*/face` and `demo/*` and recurses with `$(MAKE) -C ... install` so a single `make` from root builds and installs everything.

## Engine cleanup tasks

1. **Delete `wasm_load_face`** in `wasm_runtime.{c,h}` — superseded by `wasm_register_face` + `wasm_face_ensure_loaded`. The dynamic-install path (`wasm_install_commit`) switches to register+ensure.
2. **Replace per-face `extern` block in `main.c`** with the manifest loop above.
3. **Drop `firmware/main/faces/` from source control** — it's a build output dir. The shared `face.mk install` target still copies `.wasm` files there for IDF to embed.
4. **Move `font5x7.h`** out of `main/` root and into `engine/`.

## Files and dirs to delete

| Path | Reason |
|---|---|
| `tools/pixelitl_service.exs` | Superseded by `apps/dashboard/service/` |
| `sdkconfig.old` | Stale 88 KB |
| `.dexter.db*` | 24 MB local cache, not source — gitignore |
| `sdk/examples/demo.c` + `demo.wasm` | Redundant with rest of `demo/` |
| `sdk/examples/` (after move) | Empty |
| `firmware/main/faces/*.wasm` (committed copies) | Build output; gitignore the dir |
| `wasm_load_face` (in `wasm_runtime.{c,h}`) | Dead post-refactor |

## .gitignore additions

```
.dexter.db*
firmware/build/
firmware/main/faces/*.wasm
apps/*/face/*.wasm
apps/*/peer/build/
apps/*/service/_build/
demo/*/*.wasm
studio/_build/
```

## Migration order (low-risk → high-risk)

Each step ends with a build (and where applicable, flash) verification before moving on.

1. **Spec written and committed** (this document). No code moves.
2. **Move firmware to `firmware/`** — `git mv` IDF-managed files. Verify `cd firmware && idf.py build` succeeds.
3. **Reorganize `firmware/main/` into `engine/` + `drivers/`** — `git mv` files, update `firmware/main/CMakeLists.txt` `SRCS`. Verify build.
4. **Create `sdk/face.mk` and `sdk/README.md`.** Verify a single face still builds via the new shared rule.
5. **Move apps:** `sdk/examples/{dashboard,glucose,store,radar,weather}.c` → `apps/<name>/face/<name>.c`. Move `service/` → `apps/dashboard/service/`. Move `peer/` → `apps/radar/peer/`. Per-app Makefile in each `face/` dir. Verify `make -C apps/dashboard/face install` produces the embedded binary at the expected location.
6. **Move demos:** `sdk/examples/{plasma,clock,nyan,holi,buttons,invaders,hd_demo,ultra_demo}.c` → `demo/<name>/<name>.c` with Makefile.
7. **Add `firmware/main/embedded_faces.txt` + CMake glue, replace `main.c` extern block with the loop.** Verify firmware boots and all 4 embedded faces work.
8. **Delete dead code/files:** `wasm_load_face`, `sdk/examples/Makefile`, `sdk/examples/`, `tools/pixelitl_service.exs`, `sdkconfig.old`.
9. **Update `.gitignore`, `README.md`, `ARCHITECTURE.md`** to match the new layout.
10. **Top-level `Makefile`** that walks apps + demo and builds everything.

## Out of scope

- No changes to face SDK API (`pixelitl.h` stays as-is).
- No changes to studio internals, just keep it at top level.
- No changes to wasm host functions or face protocol.
- No CI/release pipeline — there isn't one yet.
- No build system rewrite for IDF or Phoenix subprojects, only paths and embedded-face wiring.

## Verification criteria

After all steps complete:

- `cd firmware && idf.py build flash` succeeds and the device boots with all 4 embedded faces working (swipe between them).
- `make` from repo root builds every face and installs the 4 embedded ones into `firmware/main/faces/`.
- `make -C apps/radar/peer && cd apps/radar/peer && idf.py build` succeeds.
- `cd apps/dashboard/service && mix compile` succeeds.
- `cd studio && mix compile` succeeds.
- `git grep wasm_load_face` returns nothing.
- Repo root `ls` shows only README/docs + the subproject dirs (no IDF artifacts).
