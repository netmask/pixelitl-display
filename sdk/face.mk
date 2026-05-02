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
