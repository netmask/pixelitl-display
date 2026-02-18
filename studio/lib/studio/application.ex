defmodule Studio.Application do
  @moduledoc false

  use Application

  @impl true
  def start(_type, _args) do
    children = [
      StudioWeb.Telemetry,
      Studio.Repo,
      {Ecto.Migrator,
       repos: Application.fetch_env!(:studio, :ecto_repos), skip: skip_migrations?()},
      {DNSCluster, query: Application.get_env(:studio, :dns_cluster_query) || :ignore},
      {Phoenix.PubSub, name: Studio.PubSub},
      Studio.Compiler,
      StudioWeb.Endpoint
    ]

    opts = [strategy: :one_for_one, name: Studio.Supervisor]
    Supervisor.start_link(children, opts)
  end

  @impl true
  def config_change(changed, _new, removed) do
    StudioWeb.Endpoint.config_change(changed, removed)
    :ok
  end

  defp skip_migrations?() do
    System.get_env("RELEASE_NAME") == nil
  end
end
