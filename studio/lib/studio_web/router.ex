defmodule StudioWeb.Router do
  use StudioWeb, :router

  pipeline :browser do
    plug :accepts, ["html"]
    plug :fetch_session
    plug :fetch_live_flash
    plug :put_root_layout, html: {StudioWeb.Layouts, :root}
    plug :protect_from_forgery
    plug :put_secure_browser_headers
  end

  pipeline :api do
    plug :accepts, ["json", "text", "wasm"]
  end

  scope "/", StudioWeb do
    pipe_through :browser

    get "/", PageController, :home

    live "/simulator", SimulatorLive
    live "/editor", EditorLive
    live "/editor/:id", EditorLive
    live "/faces", FacesLive
  end

  scope "/api/faces", StudioWeb do
    pipe_through :api

    get "/index.txt", FaceController, :index_txt
    get "/:slug", FaceController, :wasm
  end
end
