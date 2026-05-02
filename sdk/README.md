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
