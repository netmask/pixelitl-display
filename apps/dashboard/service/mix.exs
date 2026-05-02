defmodule PixelitlService.MixProject do
  use Mix.Project

  def project do
    [
      app: :pixelitl_service,
      version: "0.1.0",
      elixir: "~> 1.19",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      releases: releases()
    ]
  end

  def application do
    [
      extra_applications: [:logger, :crypto, :public_key, :ssl, :inets],
      mod: {PixelitlService.Application, []}
    ]
  end

  defp deps do
    [
      {:bandit, "~> 1.10"},
      {:plug, "~> 1.19"},
      {:jason, "~> 1.4"},
      {:req, "~> 0.5"}
    ]
  end

  defp releases do
    [
      pixelitl_service: [
        include_executables_for: [:unix],
        steps: [:assemble]
      ]
    ]
  end
end
