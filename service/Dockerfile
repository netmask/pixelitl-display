# --- Stage 1: build the release ---
FROM hexpm/elixir:1.19-erlang-28.5-debian-bookworm-20260421-slim AS build

ENV MIX_ENV=prod \
    LANG=C.UTF-8

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
        build-essential git ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Hex/rebar
RUN mix local.hex --force && mix local.rebar --force

# Deps first for layer caching
COPY mix.exs ./
RUN mix deps.get --only prod && \
    mix deps.compile

# App sources
COPY config ./config
COPY lib ./lib
COPY priv ./priv

RUN mix release

# --- Stage 2: minimal runtime ---
FROM debian:bookworm-slim AS runtime

ENV LANG=C.UTF-8 \
    PIXELITL_PYTHON=python3

# Python + Pillow for rasterize_stdin.py; libstdc++/libssl/ncurses for Erlang.
RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
        python3 python3-pil \
        libstdc++6 libssl3 ncurses-bin libgcc-s1 libtinfo6 \
        ca-certificates tini && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Cache dir — when running on Fly with a volume mount at /data, switch via
# PIXELITL_CACHE_DIR=/data/cache. Pre-create both for safety.
RUN mkdir -p /app/.cache/png /app/.cache/sprite /data/cache/png /data/cache/sprite

COPY --from=build /app/_build/prod/rel/pixelitl_service ./

EXPOSE 4001

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["bin/pixelitl_service", "start"]
