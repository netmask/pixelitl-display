defmodule PixelitlService.Router do
  use Plug.Router
  require Logger

  alias PixelitlService.{HN, Image, State}

  plug :log_req
  plug :match
  plug :dispatch

  defp log_req(conn, _opts) do
    peer =
      case conn.remote_ip do
        {a, b, c, d} -> "#{a}.#{b}.#{c}.#{d}"
        other -> inspect(other)
      end

    Logger.info("[req] #{conn.method} #{conn.request_path} from #{peer}")
    conn
  end

  get "/health", do: json(conn, %{ok: true})

  get "/hn" do
    case HN.fetch() do
      {:error, err} ->
        Logger.error("[hn] returning 502: #{inspect(err)}")

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
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 16)

    case Image.sprite_for(prompt, size, palette) do
      {:ok, sprite} -> json(conn, sprite)
      {:error, err} -> json_err(conn, 502, err)
    end
  end

  get "/image/sprite/top" do
    conn = fetch_query_params(conn)
    size = conn.params["size"] |> to_int(48) |> clamp(16, 96)
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 16)

    prompt =
      case HN.fetch() do
        [first | _] -> first["title"] || "mystery"
        _ -> "mystery"
      end

    case Image.sprite_for(prompt, size, palette) do
      {:ok, sprite} -> json(conn, Map.put(sprite, "prompt", prompt))
      {:error, err} -> json_err(conn, 502, err)
    end
  end

  # Per-HN-index icon, binary wire format the device parses directly:
  #   [u16 W][u16 H][u16 N][u16 pal * N][u8 data * W*H], all little-endian.
  get "/image/sprite/hn/:index" do
    conn = fetch_query_params(conn)
    idx = conn.params["index"] |> to_int(-1)
    size = conn.params["size"] |> to_int(48) |> clamp(16, 96)
    palette = conn.params["palette"] |> to_int(10) |> clamp(2, 16)

    stories =
      case HN.fetch() do
        s when is_list(s) -> s
        _ -> []
      end

    cond do
      idx < 0 or idx >= length(stories) ->
        send_resp(conn, 404, "index out of range")

      true ->
        prompt = Enum.at(stories, idx)["title"] || "mystery"

        case Image.sprite_for(prompt, size, palette) do
          {:ok, sprite} ->
            bin = Image.sprite_to_binary(sprite)

            conn
            |> put_resp_content_type("application/octet-stream")
            |> send_resp(200, bin)

          {:error, err} ->
            json_err(conn, 502, err)
        end
    end
  end

  get "/dota/status", do: json(conn, %{alert: State.dota_get()})

  post "/dota/trigger" do
    State.dota_set(true)
    Logger.info("[dota] TRIGGERED")
    json(conn, %{ok: true, alert: true})
  end

  post "/dota/clear" do
    was = State.dota_get()
    State.dota_set(false)
    if was, do: Logger.info("[dota] cleared"), else: :ok
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
