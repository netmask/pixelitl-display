import Config

# Dev: pick up local env vars and use a local cache dir.
config :pixelitl_service,
  port: String.to_integer(System.get_env("PORT") || "4001"),
  cache_dir: System.get_env("PIXELITL_CACHE_DIR") || Path.expand("../.cache", __DIR__),
  openai_api_key: System.get_env("OPENAI_API_KEY") || "",
  openai_model: System.get_env("OPENAI_MODEL") || "gpt-5.4-mini",
  image_model: System.get_env("OPENAI_IMAGE_MODEL") || "gpt-image-1",
  python_bin: System.get_env("PIXELITL_PYTHON") || "python3"
