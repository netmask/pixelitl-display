# pixelitl-service

Phoenix-style HTTP service for the pixelitl display:

- `GET /hn` — top 10 HN stories with GPT-5.4-mini summaries (≤110 char each)
- `GET /image/sprite/hn/:index` — gpt-image-1 emblem rasterized to a 48×48 RGB565 palette-indexed sprite, returned as a binary blob the device parses directly
- `GET /image/sprite?prompt=...` — same, freeform prompt (JSON)
- `GET /image/sprite/top` — same, prompt = current top HN headline (JSON)
- `GET /dota/status` / `POST /dota/trigger` / `POST /dota/clear` — alert flag for the device
- `GET /` — bookmark-friendly trigger page
- `GET /health` — liveness

## Layout

```
service/
├── lib/pixelitl_service/    # Application, State, HN, Image, Router
├── priv/rasterize_stdin.py  # PNG → palette-indexed sprite JSON (Pillow)
├── config/                  # config.exs / dev / prod / runtime
├── Dockerfile               # multi-stage build, ~120 MB runtime image
├── fly.toml                 # Fly machine config + volume mount
└── mix.exs
```

## Local dev

```fish
cd service

# one-time
mix deps.get

# run
set -x OPENAI_API_KEY sk-...
mix run --no-halt
```

You'll see something like `pixelitl-service starting on :4001` and `cache_dir = .../service/.cache`.

## Deploy to Fly.io

One-time setup:

```fish
brew install flyctl
fly auth login

# from inside service/:
cd service

# 1. Create the app + volume + secret
fly launch --no-deploy   # accept defaults, or rename app= in fly.toml
fly volumes create pixelitl_data --size 1 --region iad
fly secrets set OPENAI_API_KEY=sk-...

# 2. Deploy
fly deploy

# 3. Confirm
fly status
curl https://<your-app>.fly.dev/health
curl https://<your-app>.fly.dev/dota/status
```

Subsequent deploys: `fly deploy`.

## Environment variables

| Var | Default | Notes |
|-----|---------|-------|
| `PORT` | `4001` | listen port |
| `PIXELITL_CACHE_DIR` | `/data/cache` (prod) / `./.cache` (dev) | sprite + png cache root |
| `OPENAI_API_KEY` | (none) | summaries fall back to truncated titles if missing |
| `OPENAI_MODEL` | `gpt-5.4-mini` | chat model for HN summaries |
| `OPENAI_IMAGE_MODEL` | `gpt-image-1` | image gen model |
| `HN_TTL_SECONDS` | `300` | in-memory cache TTL for `/hn` |
| `SPRITE_TTL_SECONDS` | `3600` | in-memory cache TTL for sprites (disk cache is forever) |

## Cache layout

```
$PIXELITL_CACHE_DIR/
├── png/<sha>.png        original 1024x1024 OpenAI output
└── sprite/<sha>.bin     palette-indexed sprite, exact wire format
    sprite/<sha>.json    sidecar: {prompt, size, palette, sha}
```

`sha` = first 16 hex chars of `sha256("<prompt>|<size>|<palette>")`. Cache survives restarts when the volume is mounted (`/data` on Fly).

## Pointing the device at the deployed service

The dashboard face reads `proxy_url` from KV. Set it to your Fly URL:

```fish
# whatever mechanism the device exposes for KV writes — e.g. via the studio UI
proxy_url = "https://<your-app>.fly.dev"
```

The device now polls Fly directly. Latency goes from ~5 ms (LAN) to ~30–80 ms (cloud) per fetch — fine for the dashboard's 5-10 s cadence.
