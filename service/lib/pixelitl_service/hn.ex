defmodule PixelitlService.HN do
  @moduledoc """
  Hacker News fetcher. Pulls top 10 front-page stories via Algolia and rewrites
  each headline through OpenAI into a ≤110-char one-sentence summary.
  Caches in-memory by TTL.
  """

  require Logger

  alias PixelitlService.State

  def fetch do
    ttl = Application.fetch_env!(:pixelitl_service, :hn_ttl_seconds)
    {cached, at} = State.hn_get()
    now = System.monotonic_time(:second)

    if cached && now - at < ttl do
      Logger.info("[hn] cache hit (age #{now - at}s / ttl #{ttl}s, #{length(cached)} stories)")
      cached
    else
      Logger.info("[hn] cache miss — fetching from Algolia")

      case fetch_algolia() do
        {:ok, hits} ->
          Logger.info("[hn] algolia returned #{length(hits)} hits; summarizing...")
          t0 = System.monotonic_time(:millisecond)

          stories =
            hits
            |> Enum.with_index()
            |> Enum.map(fn {h, idx} ->
              title = h["title"] || ""
              summary = summarize(title)

              Logger.info(
                "[hn]  #{idx + 1}. (#{h["points"] || 0}pt) " <>
                  ~s("#{String.slice(title, 0, 60)}") <> " -> " <> ~s("#{summary}")
              )

              %{
                "title" => String.slice(title, 0, 80),
                "summary" => summary,
                "points" => h["points"] || 0
              }
            end)

          dt = System.monotonic_time(:millisecond) - t0
          Logger.info("[hn] summarized #{length(stories)} stories in #{dt}ms")
          State.hn_put(stories)
          stories

        {:error, e} = err ->
          Logger.error("[hn] ALGOLIA FAILED: #{inspect(e)}")
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

    case Req.get("https://hn.algolia.com/api/v1/search",
           params: params,
           receive_timeout: 10_000
         ) do
      {:ok, %Req.Response{status: 200, body: %{"hits" => hits}}} -> {:ok, hits}
      {:ok, resp} -> {:error, {:status, resp.status}}
      {:error, e} -> {:error, e}
    end
  end

  defp summarize(title) do
    key = openai_key()

    if key == "" do
      fallback(title)
    else
      case openai_summarize(key, title) do
        {:ok, s} ->
          clip(s)

        {:error, e} ->
          Logger.warning(
            "[openai] error for " <> ~s("#{String.slice(title, 0, 50)}") <>
              ": #{inspect(e)} — falling back"
          )
          fallback(title)

        other ->
          Logger.warning("[openai] unexpected: #{inspect(other)} — falling back")
          fallback(title)
      end
    end
  end

  defp openai_summarize(key, title) do
    model = Application.fetch_env!(:pixelitl_service, :openai_model)

    body = %{
      model: model,
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
      {:ok,
       %Req.Response{
         status: 200,
         body: %{"choices" => [%{"message" => %{"content" => c}} | _]}
       }} ->
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
    if String.length(title) <= 110 do
      title
    else
      cut = String.slice(title, 0, 110)

      case String.split(cut, " ") |> Enum.drop(-1) do
        [] -> cut
        words -> Enum.join(words, " ")
      end
    end
  end

  defp openai_key do
    Application.get_env(:pixelitl_service, :openai_api_key, "") |> String.trim()
  end
end
