CC = clang
CFLAGS = --target=wasm32 -O2 -nostdlib -fno-builtin \
    -Wl,--no-entry \
    -Wl,--export=app_init \
    -Wl,--export=app_update \
    -Wl,--export=app_draw \
    -Wl,--allow-undefined \
    -Wl,--initial-memory=131072 \
    -Wl,--max-memory=262144 \
    -I..

FACES = plasma.wasm clock.wasm demo.wasm hd_demo.wasm ultra_demo.wasm weather.wasm buttons.wasm store.wasm glucose.wasm

all: $(FACES)

%.wasm: %.c ../pixelitl.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(FACES)

install: $(FACES)
	mkdir -p ../../main/faces
	cp $(FACES) ../../main/faces/

.PHONY: all clean install
