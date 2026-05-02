defmodule PixelitlService.Image do
  @moduledoc """
  Generates an image via gpt-image-1, then rasterizes it into a palette-indexed
  RGB565 sprite the device can blit. Cached in memory and on disk.
  """

  require Logger
  alias PixelitlService.State

  def sprite_for(prompt, size, palette) do
    ttl = Application.fetch_env!(:pixelitl_service, :sprite_ttl_seconds)
    key = {prompt, size, palette}
    sha = key_sha(prompt, size, palette)

    case State.sprite_get(key) do
      {sprite, at} ->
        age = System.monotonic_time(:second) - at

        if age < ttl do
          Logger.info("[img] memory hit (age #{age}s) prompt=" <> ~s("#{String.slice(prompt, 0, 60)}"))
          {:ok, sprite}
        else
          Logger.info("[img] memory stale (age #{age}s) — checking disk/openai")
          generate_and_cache(prompt, size, palette, key, sha)
        end

      nil ->
        case load_from_disk(sha) do
          {:ok, sprite} ->
            Logger.info("[img] disk hit (#{sha}) prompt=" <> ~s("#{String.slice(prompt, 0, 60)}"))
            State.sprite_put(key, sprite)
            {:ok, sprite}

          :miss ->
            Logger.info("[img] full miss — generating")
            generate_and_cache(prompt, size, palette, key, sha)
        end
    end
  end

  defp generate_and_cache(prompt, size, palette, key, sha) do
    with {:ok, png_bytes} <- openai_generate(prompt),
         :ok <- save_png(sha, png_bytes),
         {:ok, sprite} <- rasterize(png_bytes, size, palette),
         :ok <- save_sprite(sha, prompt, size, palette, sprite) do
      State.sprite_put(key, sprite)
      {:ok, sprite}
    end
  end

  # ---- OpenAI ----

  defp openai_generate(prompt) do
    key = Application.get_env(:pixelitl_service, :openai_api_key, "") |> String.trim()
    model = Application.fetch_env!(:pixelitl_service, :image_model)

    if key == "" do
      {:error, :no_api_key}
    else
      wrapped =
        "Bold pixel-art emblem on a plain background. " <>
          "Thick silhouette, strong color blocks, minimal fine detail, readable at small size. " <>
          "No text, no gradient. Subject: " <> prompt

      body = %{model: model, prompt: wrapped, size: "1024x1024", n: 1}

      Logger.info("[img] requesting #{model} for: " <> ~s("#{String.slice(prompt, 0, 80)}"))
      t0 = System.monotonic_time(:millisecond)

      case Req.post("https://api.openai.com/v1/images/generations",
             json: body,
             headers: [{"authorization", "Bearer " <> key}],
             receive_timeout: 120_000
           ) do
        {:ok, %Req.Response{status: 200, body: %{"data" => [%{"b64_json" => b64} | _]}}} ->
          dt = System.monotonic_time(:millisecond) - t0
          Logger.info("[img] gpt-image-1 ok in #{dt}ms")
          {:ok, Base.decode64!(b64)}

        {:ok, %Req.Response{status: 200, body: %{"data" => [%{"url" => url} | _]}}} ->
          Logger.info("[img] gpt-image-1 returned URL, fetching")

          case Req.get(url, receive_timeout: 30_000) do
            {:ok, %Req.Response{status: 200, body: png}} -> {:ok, png}
            other -> {:error, {:url_fetch, inspect(other)}}
          end

        {:ok, resp} ->
          Logger.error("[img] OpenAI error: #{resp.status} #{inspect(resp.body)}")
          {:error, {:openai, resp.status, resp.body}}

        {:error, e} ->
          Logger.error("[img] OpenAI transport error: #{inspect(e)}")
          {:error, e}
      end
    end
  end

  # ---- Rasterize via Python helper ----

  defp rasterize(png_bytes, size, palette) do
    tmp = Path.join(System.tmp_dir!(), "pxl_gen_#{System.os_time(:millisecond)}.png")
    File.write!(tmp, png_bytes)

    try do
      python = Application.fetch_env!(:pixelitl_service, :python_bin)

      args = [
        script_path(),
        tmp,
        "--size",
        Integer.to_string(size),
        "--palette",
        Integer.to_string(palette)
      ]

      case System.cmd(python, args, stderr_to_stdout: false) do
        {stdout, 0} ->
          case Jason.decode(stdout) do
            {:ok, sprite} -> {:ok, sprite}
            {:error, e} -> {:error, {:decode, inspect(e), String.slice(stdout, 0, 200)}}
          end

        {stdout, code} ->
          {:error, {:python, code, stdout}}
      end
    after
      File.rm(tmp)
    end
  end

  defp script_path do
    Application.app_dir(:pixelitl_service, "priv/rasterize_stdin.py")
  end

  # ---- Disk cache ----

  defp cache_root, do: Application.fetch_env!(:pixelitl_service, :cache_dir)
  defp png_dir, do: Path.join(cache_root(), "png")
  defp spr_dir, do: Path.join(cache_root(), "sprite")

  defp ensure_dirs do
    File.mkdir_p!(png_dir())
    File.mkdir_p!(spr_dir())
  end

  defp key_sha(prompt, size, palette) do
    data = "#{prompt}|#{size}|#{palette}"

    :crypto.hash(:sha256, data)
    |> Base.encode16(case: :lower)
    |> String.slice(0, 16)
  end

  defp save_png(sha, png_bytes) do
    ensure_dirs()
    path = Path.join(png_dir(), sha <> ".png")
    File.write!(path, png_bytes)
    Logger.info("[img] saved #{path} (#{byte_size(png_bytes)} bytes)")
    :ok
  end

  defp save_sprite(sha, prompt, size, palette, sprite) do
    ensure_dirs()
    bin_path = Path.join(spr_dir(), sha <> ".bin")
    meta_path = Path.join(spr_dir(), sha <> ".json")

    bin = sprite_to_binary(sprite)

    File.write!(bin_path, bin)

    File.write!(
      meta_path,
      Jason.encode!(%{prompt: prompt, size: size, palette: palette, sha: sha})
    )

    Logger.info("[img] saved #{bin_path} (#{byte_size(bin)} bytes)")
    :ok
  end

  defp load_from_disk(sha) do
    bin_path = Path.join(spr_dir(), sha <> ".bin")

    case File.read(bin_path) do
      {:ok, <<w::16-little, h::16-little, n::16-little, rest::binary>>}
      when byte_size(rest) == n * 2 + w * h ->
        <<pal_bin::binary-size(n * 2), data_bin::binary-size(w * h)>> = rest
        palette = for <<c::16-little <- pal_bin>>, do: c
        data = for <<x::8 <- data_bin>>, do: x
        {:ok, %{"w" => w, "h" => h, "palette" => palette, "data" => data}}

      _ ->
        :miss
    end
  end

  @doc "Pack a sprite map into the binary wire format the device parses."
  def sprite_to_binary(%{"w" => w, "h" => h, "palette" => pal, "data" => data}) do
    n = length(pal)
    pal_bin = for c <- pal, into: <<>>, do: <<c::16-little>>
    data_bin = for x <- data, into: <<>>, do: <<x::8>>
    <<w::16-little, h::16-little, n::16-little, pal_bin::binary, data_bin::binary>>
  end
end
