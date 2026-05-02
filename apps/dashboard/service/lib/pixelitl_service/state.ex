defmodule PixelitlService.State do
  @moduledoc """
  In-memory state: dota alert flag, HN cache, and a hot sprite cache.
  Disk cache lives in PixelitlService.Image.
  """

  use Agent

  def start_link(_),
    do:
      Agent.start_link(
        fn -> %{dota_alert: false, hn: nil, hn_at: 0, sprites: %{}} end,
        name: __MODULE__
      )

  def dota_set(v), do: Agent.update(__MODULE__, &Map.put(&1, :dota_alert, v))
  def dota_get, do: Agent.get(__MODULE__, & &1.dota_alert)

  def hn_get, do: Agent.get(__MODULE__, fn s -> {s.hn, s.hn_at} end)

  def hn_put(stories),
    do:
      Agent.update(__MODULE__, fn s ->
        %{s | hn: stories, hn_at: System.monotonic_time(:second)}
      end)

  def sprite_get(key) do
    Agent.get(__MODULE__, fn s -> Map.get(s.sprites, key) end)
  end

  def sprite_put(key, sprite) do
    Agent.update(__MODULE__, fn s ->
      %{s | sprites: Map.put(s.sprites, key, {sprite, System.monotonic_time(:second)})}
    end)
  end
end
