import Config

# Sensible defaults — overridden in dev.exs / runtime.exs by env vars.
config :pixelitl_service,
  port: 4001,
  cache_dir: "./.cache",
  openai_api_key: "",
  openai_model: "gpt-5.4-mini",
  image_model: "gpt-image-1",
  hn_ttl_seconds: 300,
  sprite_ttl_seconds: 3600,
  python_bin: "python3"

config :logger, :console,
  format: "$time $metadata[$level] $message\n",
  metadata: []

import_config "#{config_env()}.exs"
