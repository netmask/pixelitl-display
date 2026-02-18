defmodule StudioWeb.FacesLive do
  use StudioWeb, :live_view

  alias Studio.Faces

  @impl true
  def mount(_params, _session, socket) do
    {:ok, assign(socket, faces: Faces.list_faces(), page_title: "Faces")}
  end

  @impl true
  def handle_event("toggle_published", %{"id" => id}, socket) do
    face = Faces.get_face!(id)
    {:ok, _face} = Faces.toggle_published(face)
    {:noreply, assign(socket, faces: Faces.list_faces())}
  end

  def handle_event("delete", %{"id" => id}, socket) do
    face = Faces.get_face!(id)
    {:ok, _} = Faces.delete_face(face)
    {:noreply, assign(socket, faces: Faces.list_faces())}
  end

  def handle_event("upload_wasm", _params, socket) do
    {:noreply, socket}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="max-w-4xl mx-auto p-6">
      <div class="flex items-center justify-between mb-6">
        <h1 class="text-2xl font-bold">Faces</h1>
        <.link navigate={~p"/editor"} class="btn btn-primary btn-sm">
          <.icon name="hero-plus" class="size-4" /> New Face
        </.link>
      </div>

      <div class="overflow-x-auto">
        <table class="table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Slug</th>
              <th>Language</th>
              <th>Size</th>
              <th>Published</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr :for={face <- @faces} class="hover">
              <td>
                <.link navigate={~p"/editor/#{face.id}"} class="link link-primary">
                  {face.name}
                </.link>
              </td>
              <td class="font-mono text-sm">{face.slug}</td>
              <td class="uppercase text-sm">{face.language}</td>
              <td>
                {if face.wasm_size > 0, do: "#{Float.round(face.wasm_size / 1024, 1)} KB", else: "-"}
              </td>
              <td>
                <input
                  type="checkbox"
                  class="toggle toggle-sm toggle-primary"
                  checked={face.published}
                  phx-click="toggle_published"
                  phx-value-id={face.id}
                />
              </td>
              <td class="flex gap-2">
                <.link navigate={~p"/editor/#{face.id}"} class="btn btn-ghost btn-xs">
                  <.icon name="hero-pencil" class="size-4" />
                </.link>
                <button
                  phx-click="delete"
                  phx-value-id={face.id}
                  data-confirm="Delete this face?"
                  class="btn btn-ghost btn-xs text-error"
                >
                  <.icon name="hero-trash" class="size-4" />
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <div :if={@faces == []} class="text-center py-12 text-base-content/50">
        No faces yet. Create one or run seeds.
      </div>
    </div>
    """
  end
end
