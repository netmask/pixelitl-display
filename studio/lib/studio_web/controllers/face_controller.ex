defmodule StudioWeb.FaceController do
  use StudioWeb, :controller

  alias Studio.Faces

  def index_txt(conn, _params) do
    index = Faces.generate_index_txt()

    conn
    |> put_resp_content_type("text/plain")
    |> send_resp(200, index)
  end

  def wasm(conn, %{"slug" => slug}) do
    slug = String.replace_suffix(slug, ".wasm", "")

    case Faces.get_face_by_slug(slug) do
      %{wasm_binary: wasm} when not is_nil(wasm) ->
        conn
        |> put_resp_content_type("application/wasm")
        |> put_resp_header("cache-control", "no-cache")
        |> send_resp(200, wasm)

      _ ->
        send_resp(conn, 404, "Not found")
    end
  end
end
