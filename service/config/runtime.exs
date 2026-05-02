import Config

# Runtime config — runs at boot in releases (mix release / Fly).
# All settings come from env vars so the same image works anywhere.

if config_env() == :prod do
  config :pixelitl_service,
    port: String.to_integer(System.get_env("PORT") || "4001"),
    cache_dir: System.get_env("PIXELITL_CACHE_DIR") || "/data/cache",
    openai_api_key: System.get_env("OPENAI_API_KEY") || "",
    openai_model: System.get_env("OPENAI_MODEL") || "gpt-5.4-mini",
    image_model: System.get_env("OPENAI_IMAGE_MODEL") || "gpt-image-1",
    hn_ttl_seconds: String.to_integer(System.get_env("HN_TTL_SECONDS") || "300"),
    sprite_ttl_seconds: String.to_integer(System.get_env("SPRITE_TTL_SECONDS") || "3600"),
    python_bin: System.get_env("PIXELITL_PYTHON") || "python3"
end
