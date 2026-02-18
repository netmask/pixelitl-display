# Pixelitl Studio

Web-based development studio for creating, compiling, and deploying WASM faces to the Pixelitl Display. Built with Phoenix LiveView.

## Setup

```bash
mix setup    # install deps, create DB, run seeds
mix phx.server
```

Open http://localhost:4000 in your browser.

The server binds to `0.0.0.0:4000` so the ESP32 device can reach it over the local network.

## Features

### Editor

Write faces in **C** or **AssemblyScript** with a code editor, compile to WASM with one click, and preview instantly in the built-in simulator. Compilation errors are shown inline.

### Face Library

Browse all faces, see compiled sizes, publish or unpublish faces that the device can install.

### Simulator

Runs the same `.wasm` binaries as the ESP32 using the browser's WebAssembly engine. Supports touch input via mouse and multiple rendering modes (Flat, Grid, Soft Glow).

### Device API

The studio serves a REST API that the device's Store face uses to browse and install faces:

- `GET /api/faces/index.txt` — face catalog (`Name|filename.wasm` per line)
- `GET /api/faces/:filename` — download a compiled `.wasm` binary

## Compilers

- **C** — Uses LLVM clang with `--target=wasm32` (requires `brew install llvm lld`)
- **AssemblyScript** — Uses `asc` from the AssemblyScript compiler (requires `npm install -g assemblyscript`)

## Seeding

```bash
mix run priv/repo/seeds.exs
```

Seeds all example faces from `sdk/examples/` with their source code and compiled WASM binaries.
