defmodule PixelitlService.Application do
  @moduledoc false
  use Application

  require Logger

  @impl true
  def start(_type, _args) do
    port = Application.fetch_env!(:pixelitl_service, :port)
    cache_dir = Application.fetch_env!(:pixelitl_service, :cache_dir)

    File.mkdir_p!(Path.join(cache_dir, "png"))
    File.mkdir_p!(Path.join(cache_dir, "sprite"))

    Logger.info("pixelitl-service starting on :#{port}")
    Logger.info("  cache_dir = #{cache_dir}")

    children = [
      PixelitlService.State,
      {Bandit, plug: PixelitlService.Router, port: port, ip: {0, 0, 0, 0}}
    ]

    opts = [strategy: :one_for_one, name: PixelitlService.Supervisor]
    Supervisor.start_link(children, opts)
  end
end
