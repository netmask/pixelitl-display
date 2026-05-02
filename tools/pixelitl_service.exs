# pixelitl-service — single-file Phoenix Playground app.
#
# Run:
#   elixir tools/pixelitl_service.exs
#
# Optional env:
#   OPENAI_API_KEY   — if set, HN headlines are summarized to ≤32 chars via OpenAI.
#                      Without it, summaries fall back to truncated titles.
#   OPENAI_MODEL     — default "gpt-4o-mini".
#   HN_TTL_SECONDS   — default 300.
#   PORT             — default 4001 (OrbStack squats on 8080 on this machine).
#
# Endpoints:
#   GET  /                          — HTML page with a big DOTA TIME button
#   GET  /hn                        — {"stories":[{"title","summary","points"}, ...]}
#   GET  /dota/status               — {"alert": bool}
#   POST /dota/trigger              — sets alert flag
#   POST /dota/clear                — clears flag
#   GET  /image/sprite?prompt=X     — {"w","h","palette":[rgb565..],"data":[..]}
#                                     generates an image via gpt-image-1, rasterizes
#                                     to a palette-indexed sprite fit for the device.
#                                     Optional: &size=48 &palette=10
#   GET  /image/sprite/top          — same, prompt = today's top HN headline
#   GET  /health                    — liveness

Mix.install([
  {:bandit, "~> 1.10"},
  {:plug, "~> 1.19"},
  {:jason, "~> 1.4"},
  {:req, "~> 0.5"}
])

# ---------- In-memory state (dota flag + HN cache) ----------

defmodule Pixelitl.State do
  use Agent

  def start_link(_),
    do:
      Agent.start_link(
        fn -> %{dota_alert: false, hn: nil, hn_at: 0} end,
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

  # Simple sprite cache — key is {prompt, size, palette}, value is {sprite_map, at}.
  def sprite_get(key) do
    Agent.get(__MODULE__, fn s ->
      cache = Map.get(s, :sprites, %{})
      Map.get(cache, key)
    end)
  end

  def sprite_put(key, sprite) do
    Agent.update(__MODULE__, fn s ->
      cache = Map.get(s, :sprites, %{})
      Map.put(s, :sprites, Map.put(cache, key, {sprite, System.monotonic_time(:second)}))
    end)
  end
end

# ---------- HN fetch + OpenAI summarization ----------

defmodule Pixelitl.HN do
  @ttl String.to_integer(System.get_env("HN_TTL_SECONDS", "300"))
  @model System.get_env("OPENAI_MODEL", "gpt-5.4-mini")

  def fetch do
    {cached, at} = Pixelitl.State.hn_get()
    now = System.monotonic_time(:second)

    if cached && now - at < @ttl do
      IO.puts("[hn] cache hit (age #{now - at}s / ttl #{@ttl}s, #{length(cached)} stories)")
      cached
    else
      IO.puts("[hn] cache miss — fetching from Algolia")

      case fetch_algolia() do
        {:ok, hits} ->
          IO.puts("[hn] algolia returned #{length(hits)} hits; summarizing...")
          t0 = System.monotonic_time(:millisecond)

          stories =
            hits
            |> Enum.with_index()
            |> Enum.map(fn {h, idx} ->
              title = h["title"] || ""
              summary = summarize(title)
              IO.puts("[hn]  #{idx + 1}. (#{h["points"] || 0}pt) \"#{String.slice(title, 0, 60)}\" -> \"#{summary}\"")

              %{
                "title" => String.slice(title, 0, 80),
                "summary" => summary,
                "points" => h["points"] || 0
              }
            end)

          dt = System.monotonic_time(:millisecond) - t0
          IO.puts("[hn] summarized #{length(stories)} stories in #{dt}ms")
          Pixelitl.State.hn_put(stories)
          stories

        {:error, e} = err ->
          IO.puts("[hn] ALGOLIA FAILED: #{inspect(e)}")
          if cached, do: cached, else: err
      end
    end
  end

  defp fetch_algolia do
    params = [
      tags: "front_page",
      hitsPerPage: 10,
      attributesToRetrieve: ~s(["title","points","url","objectID"]),
      attributesToHighlight: "[]"
    ]

    case Req.get("https://hn.algolia.com/api/v1/search", params: params, receive_timeout: 10_000) do
      {:ok, %Req.Response{status: 200, body: %{"hits" => hits}}} -> {:ok, hits}
      {:ok, resp} -> {:error, {:status, resp.status}}
      {:error, e} -> {:error, e}
    end
  end

  defp summarize(title) do
    key = System.get_env("OPENAI_API_KEY", "") |> String.trim()

    if key == "" do
      fallback(title)
    else
      case openai_summarize(key, title) do
        {:ok, s} ->
          clip(s)

        {:error, e} ->
          IO.puts("[openai] error for \"#{String.slice(title, 0, 50)}\": #{inspect(e)} — falling back")
          fallback(title)

        other ->
          IO.puts("[openai] unexpected: #{inspect(other)} — falling back")
          fallback(title)
      end
    end
  end

  defp openai_summarize(key, title) do
    body = %{
      model: @model,
      messages: [
        %{
          role: "system",
          content:
            "Rewrite an HN headline as a concrete one-sentence summary. " <>
              "Hard cap: 110 characters. No hype, no filler, no quotes, no trailing period. " <>
              "Use plain language. Output only the summary sentence."
        },
        %{role: "user", content: title}
      ],
      max_tokens: 80,
      temperature: 0.2
    }

    case Req.post("https://api.openai.com/v1/chat/completions",
           json: body,
           headers: [{"authorization", "Bearer " <> key}],
           receive_timeout: 15_000
         ) do
      {:ok, %Req.Response{status: 200, body: %{"choices" => [%{"message" => %{"content" => c}} | _]}}} ->
        {:ok, c |> String.trim() |> String.trim(~s("))}

      {:ok, r} ->
        {:error, {:status, r.status}}

      e ->
        e
    end
  end

  defp clip(s) do
    s = String.replace(s, ~r/\s+/, " ") |> String.trim()
    if String.length(s) > 110, do: String.slice(s, 0, 110), else: s
  end

  defp fallback(title) do
    cond do
      String.length(title) <= 110 ->
        title

      true ->
        cut = String.slice(title, 0, 110)

        case String.split(cut, " ") |> Enum.drop(-1) do
          [] -> cut
          words -> Enum.join(words, " ")
        end
    end
  end
end

# ---------- Image gen + rasterize to palette-indexed sprite ----------

defmodule Pixelitl.Image do
  @sprite_ttl 3600           # 1 hour (in-memory hot cache)
  @script Path.join([__DIR__, "rasterize_stdin.py"])
  @cache_dir Path.join([__DIR__, ".cache"])
  @png_dir   Path.join([__DIR__, ".cache", "png"])
  @spr_dir   Path.join([__DIR__, ".cache", "sprite"])
  @image_model "gpt-image-1"

  # High-level: returns %{"w","h","palette","data"} sprite map.
  # Cache layers:
  #   1. In-memory (Agent) — 1 hour TTL, fastest
  #   2. Disk cache at tools/.cache/sprite/<sha>.bin — survives restarts, free
  #   3. OpenAI — only when both caches miss
  def sprite_for(prompt, size, palette) do
    key = {prompt, size, palette}
    sha = key_sha(prompt, size, palette)

    case Pixelitl.State.sprite_get(key) do
      {sprite, at} ->
        age = System.monotonic_time(:second) - at

        if age < @sprite_ttl do
          IO.puts("[img] memory hit (age #{age}s) prompt=#{inspect(String.slice(prompt, 0, 60))}")
          {:ok, sprite}
        else
          IO.puts("[img] memory stale (age #{age}s) — checking disk/openai")
          generate_and_cache(prompt, size, palette, key, sha)
        end

      nil ->
        case load_from_disk(sha) do
          {:ok, sprite} ->
            IO.puts("[img] disk hit (#{sha}) prompt=#{inspect(String.slice(prompt, 0, 60))}")
            Pixelitl.State.sprite_put(key, sprite)
            {:ok, sprite}

          :miss ->
            IO.puts("[img] full miss — generating")
            generate_and_cache(prompt, size, palette, key, sha)
        end
    end
  end

  defp generate_and_cache(prompt, size, palette, key, sha) do
    with {:ok, png_bytes} <- openai_generate(prompt),
         :ok              <- save_png(sha, png_bytes),
         {:ok, sprite}    <- rasterize(png_bytes, size, palette),
         :ok              <- save_sprite(sha, prompt, size, palette, sprite) do
      Pixelitl.State.sprite_put(key, sprite)
      {:ok, sprite}
    end
  end

  # ---- disk cache ----

  defp ensure_dirs do
    File.mkdir_p!(@cache_dir)
    File.mkdir_p!(@png_dir)
    File.mkdir_p!(@spr_dir)
  end

  defp key_sha(prompt, size, palette) do
    data = "#{prompt}|#{size}|#{palette}"
    :crypto.hash(:sha256, data) |> Base.encode16(case: :lower) |> String.slice(0, 16)
  end

  defp save_png(sha, png_bytes) do
    ensure_dirs()
    path = Path.join(@png_dir, sha <> ".png")
    File.write!(path, png_bytes)
    IO.puts("[img] saved #{path} (#{byte_size(png_bytes)} bytes)")
    :ok
  end

  # Sprite binary layout on disk matches the /image/sprite/hn/<i> wire format:
  #   [u16 w][u16 h][u16 npal][u16 pal * npal][u8 data * w*h]
  # Plus a small sidecar JSON with the prompt for debugging.
  defp save_sprite(sha, prompt, size, palette, sprite) do
    ensure_dirs()
    bin_path = Path.join(@spr_dir, sha <> ".bin")
    meta_path = Path.join(@spr_dir, sha <> ".json")

    w = sprite["w"]
    h = sprite["h"]
    pal = sprite["palette"]
    data = sprite["data"]
    n = length(pal)
    pal_bin = for c <- pal, into: <<>>, do: <<c::16-little>>
    data_bin = for x <- data, into: <<>>, do: <<x::8>>
    bin = <<w::16-little, h::16-little, n::16-little, pal_bin::binary, data_bin::binary>>

    File.write!(bin_path, bin)
    File.write!(meta_path, Jason.encode!(%{prompt: prompt, size: size, palette: palette, sha: sha}))
    IO.puts("[img] saved #{bin_path} (#{byte_size(bin)} bytes)")
    :ok
  end

  defp load_from_disk(sha) do
    bin_path = Path.join(@spr_dir, sha <> ".bin")

    case File.read(bin_path) do
      {:ok, <<w::16-little, h::16-little, n::16-little, rest::binary>>} when byte_size(rest) == n * 2 + w * h ->
        <<pal_bin::binary-size(n * 2), data_bin::binary-size(w * h)>> = rest
        palette = for <<c::16-little <- pal_bin>>, do: c
        data = for <<x::8 <- data_bin>>, do: x
        {:ok, %{"w" => w, "h" => h, "palette" => palette, "data" => data}}

      _ ->
        :miss
    end
  end

  defp openai_generate(prompt) do
    key = System.get_env("OPENAI_API_KEY", "") |> String.trim()

    if key == "" do
      {:error, :no_api_key}
    else
      wrapped =
        "Bold pixel-art emblem on a plain background. " <>
          "Thick silhouette, strong color blocks, minimal fine detail, readable at small size. " <>
          "No text, no gradient. Subject: " <> prompt

      body = %{
        model: @image_model,
        prompt: wrapped,
        size: "1024x1024",
        n: 1
      }

      IO.puts("[img] requesting gpt-image-1 for: #{inspect(String.slice(prompt, 0, 80))}")
      t0 = System.monotonic_time(:millisecond)

      case Req.post("https://api.openai.com/v1/images/generations",
             json: body,
             headers: [{"authorization", "Bearer " <> key}],
             receive_timeout: 120_000
           ) do
        {:ok, %Req.Response{status: 200, body: %{"data" => [%{"b64_json" => b64} | _]}}} ->
          dt = System.monotonic_time(:millisecond) - t0
          IO.puts("[img] gpt-image-1 ok in #{dt}ms")
          {:ok, Base.decode64!(b64)}

        {:ok, %Req.Response{status: 200, body: %{"data" => [%{"url" => url} | _]}}} ->
          IO.puts("[img] gpt-image-1 returned URL, fetching")
          case Req.get(url, receive_timeout: 30_000) do
            {:ok, %Req.Response{status: 200, body: png}} -> {:ok, png}
            other -> {:error, {:url_fetch, inspect(other)}}
          end

        {:ok, resp} ->
          IO.puts("[img] OpenAI error: #{resp.status} #{inspect(resp.body)}")
          {:error, {:openai, resp.status, resp.body}}

        {:error, e} ->
          IO.puts("[img] OpenAI transport error: #{inspect(e)}")
          {:error, e}
      end
    end
  end

  # Write PNG to a temp file, invoke the Python rasterizer, parse JSON from stdout.
  defp rasterize(png_bytes, size, palette) do
    tmp = Path.join(System.tmp_dir!(), "pxl_gen_#{System.os_time(:millisecond)}.png")
    File.write!(tmp, png_bytes)

    try do
      python = find_python()

      args = [
        @script,
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

  # Try common Python locations in order — venv first, then system python3.
  defp find_python do
    candidates = [
      Path.join([__DIR__, ".venv", "bin", "python3"]),
      Path.join([__DIR__, ".venv", "bin", "python"]),
      "/tmp/dota_venv/bin/python3",
      "/tmp/dota_venv/bin/python",
      "python3"
    ]

    Enum.find(candidates, "python3", &File.exists?/1)
  end
end

# ---------- Router ----------

defmodule Pixelitl.Router do
  use Plug.Router

  plug :log_req
  plug :match
  plug :dispatch

  defp log_req(conn, _opts) do
    peer =
      case conn.remote_ip do
        {a, b, c, d} -> "#{a}.#{b}.#{c}.#{d}"
        other -> inspect(other)
      end

    IO.puts("[req] #{conn.method} #{conn.request_path} from #{peer}")
    conn
  end

  get "/health", do: json(conn, %{ok: true})

  get "/hn" do
    case Pixelitl.HN.fetch() do
      {:error, err} ->
        IO.puts("[hn] returning 502: #{inspect(err)}")

        conn
        |> put_resp_content_type("application/json")
        |> send_resp(502, Jason.encode!(%{stories: [], error: inspect(err)}))

      stories when is_list(stories) ->
        json(conn, %{stories: stories})
    end
  end

  get "/image/sprite" do
    conn = fetch_query_params(conn)
    prompt = conn.params["prompt"] || "mystery"
    size = conn.params["size"] |> to_int(48) |> clamp(16, 96)
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 24)

    case Pixelitl.Image.sprite_for(prompt, size, palette) do
      {:ok, sprite} -> json(conn, sprite)
      {:error, err} -> json_err(conn, 502, err)
    end
  end

  get "/image/sprite/top" do
    conn = fetch_query_params(conn)
    size = conn.params["size"] |> to_int(48) |> clamp(16, 96)
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 24)

    prompt =
      case Pixelitl.HN.fetch() do
        [first | _] -> first["title"] || "mystery"
        _ -> "mystery"
      end

    case Pixelitl.Image.sprite_for(prompt, size, palette) do
      {:ok, sprite} -> json(conn, Map.put(sprite, "prompt", prompt))
      {:error, err} -> json_err(conn, 502, err)
    end
  end

  # Per-HN-index icon in a compact binary format so the device can parse it
  # without JSON: [u16 W][u16 H][u16 N][u16 pal * N][u8 data * W*H], all LE.
  get "/image/sprite/hn/:index" do
    conn = fetch_query_params(conn)
    idx = conn.params["index"] |> to_int(-1)
    size = conn.params["size"] |> to_int(48) |> clamp(16, 96)
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 16)

    stories =
      case Pixelitl.HN.fetch() do
        s when is_list(s) -> s
        _ -> []
      end

    cond do
      idx < 0 or idx >= length(stories) ->
        send_resp(conn, 404, "index out of range")

      true ->
        prompt = Enum.at(stories, idx)["title"] || "mystery"

        case Pixelitl.Image.sprite_for(prompt, size, palette) do
          {:ok, sprite} ->
            bin = sprite_to_binary(sprite)

            conn
            |> put_resp_content_type("application/octet-stream")
            |> send_resp(200, bin)

          {:error, err} ->
            json_err(conn, 502, err)
        end
    end
  end

  defp sprite_to_binary(%{"w" => w, "h" => h, "palette" => pal, "data" => data}) do
    n = length(pal)
    pal_bin = for c <- pal, into: <<>>, do: <<c::16-little>>
    data_bin = for x <- data, into: <<>>, do: <<x::8>>
    <<w::16-little, h::16-little, n::16-little, pal_bin::binary, data_bin::binary>>
  end

  get "/dota/status", do: json(conn, %{alert: Pixelitl.State.dota_get()})

  post "/dota/trigger" do
    Pixelitl.State.dota_set(true)
    IO.puts("[dota] TRIGGERED")
    json(conn, %{ok: true, alert: true})
  end

  post "/dota/clear" do
    was = Pixelitl.State.dota_get()
    Pixelitl.State.dota_set(false)
    if was, do: IO.puts("[dota] cleared"), else: :ok
    json(conn, %{ok: true})
  end

  get "/" do
    conn
    |> put_resp_content_type("text/html")
    |> send_resp(200, index_html())
  end

  match _, do: send_resp(conn, 404, "not found")

  defp json(conn, data) do
    conn
    |> put_resp_content_type("application/json")
    |> send_resp(200, Jason.encode!(data))
  end

  defp json_err(conn, status, err) do
    conn
    |> put_resp_content_type("application/json")
    |> send_resp(status, Jason.encode!(%{error: inspect(err)}))
  end

  defp to_int(nil, default), do: default
  defp to_int(s, default) do
    case Integer.parse(s) do
      {n, _} -> n
      _ -> default
    end
  end

  defp clamp(n, lo, hi), do: max(lo, min(hi, n))

  defp index_html do
    """
    <!doctype html>
    <meta charset="utf-8">
    <title>pixelitl</title>
    <style>
      html,body{margin:0;padding:0;background:#0c1020;color:#e6e9f5;
               font-family:-apple-system,system-ui,sans-serif;height:100%;}
      body{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:24px;}
      h1{letter-spacing:4px;margin:0;}
      button{font-size:22px;padding:18px 40px;border:none;border-radius:14px;
             background:#c72840;color:white;cursor:pointer;font-weight:700;
             box-shadow:0 6px 0 #7a1a29;transition:transform .08s,box-shadow .08s;}
      button:active{transform:translateY(4px);box-shadow:0 2px 0 #7a1a29;}
      .row{display:flex;gap:12px;}
      .row button{font-size:14px;padding:10px 20px;background:#2a3254;box-shadow:0 4px 0 #141832;}
      #s{opacity:0.6;font-family:monospace;font-size:13px;}
    </style>
    <h1>PIXELITL</h1>
    <button onclick="trig()">DOTA TIME</button>
    <div class="row">
      <button onclick="clr()">clear</button>
      <button onclick="ref()">refresh status</button>
    </div>
    <div id="s">status: ?</div>
    <script>
      async function trig(){await fetch('/dota/trigger',{method:'POST'});await ref();}
      async function clr(){await fetch('/dota/clear',{method:'POST'});await ref();}
      async function ref(){const r=await fetch('/dota/status');const j=await r.json();
        document.getElementById('s').textContent='alert: '+j.alert;}
      ref();setInterval(ref,3000);
    </script>
    """
  end
end

# ---------- Boot ----------

case Pixelitl.State.start_link([]) do
  {:ok, _} -> :ok
  {:error, {:already_started, _}} -> :ok
end

port = String.to_integer(System.get_env("PORT", "4001"))

# Print the LAN IP so it's easy to copy into the device's KV.
lan_ip =
  case :inet.getifaddrs() do
    {:ok, ifs} ->
      ifs
      |> Enum.flat_map(fn {_, opts} ->
        for {:addr, {a, b, c, d}} <- opts,
            a != 127 and a != 169,
            do: "#{a}.#{b}.#{c}.#{d}"
      end)
      |> List.first() || "127.0.0.1"

    _ ->
      "127.0.0.1"
  end

IO.puts("pixelitl-service  listening on http://#{lan_ip}:#{port}  (LAN)")

if System.get_env("OPENAI_API_KEY", "") == "" do
  IO.puts("  OPENAI_API_KEY not set -- HN summaries will fall back to truncated titles")
else
  IO.puts("  summarizing with model: #{System.get_env("OPENAI_MODEL", "gpt-5.4-mini")}")
end

# Bind to 0.0.0.0 so the device (and anything else on the LAN) can reach us.
{:ok, _pid} =
  Bandit.start_link(
    plug: Pixelitl.Router,
    port: port,
    ip: {0, 0, 0, 0}
  )

Process.sleep(:infinity)
