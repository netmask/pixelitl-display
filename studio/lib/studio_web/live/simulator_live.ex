defmodule StudioWeb.SimulatorLive do
  use StudioWeb, :live_view

  alias Studio.Faces

  @impl true
  def mount(_params, _session, socket) do
    faces = Faces.list_published_faces()

    {:ok,
     assign(socket,
       faces: faces,
       selected_face_id: nil,
       page_title: "Simulator"
     )}
  end

  @impl true
  def handle_event("select_face", %{"id" => id}, socket) do
    {:noreply, assign(socket, selected_face_id: String.to_integer(id))}
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="flex flex-col items-center min-h-screen bg-base-300 p-4" id="simulator-page">
      <div class="flex items-center gap-4 mb-3">
        <h1 class="text-lg font-mono tracking-widest text-base-content/50">PIXELITL SIMULATOR</h1>
        <div class="flex gap-1">
          <.link navigate={~p"/editor"} class="btn btn-ghost btn-xs">Editor</.link>
          <.link navigate={~p"/faces"} class="btn btn-ghost btn-xs">Faces</.link>
        </div>
      </div>

      <div class="flex gap-2 items-center mb-3 flex-wrap justify-center">
        <div class="flex gap-1">
          <button
            :for={face <- @faces}
            class={[
              "btn btn-xs",
              if(face.id == @selected_face_id, do: "btn-primary", else: "btn-ghost")
            ]}
            phx-click="select_face"
            phx-value-id={face.id}
          >
            {face.name}
          </button>
        </div>
        <select id="glow-mode" class="select select-xs select-bordered">
          <option value="flat">Flat</option>
          <option value="grid" selected>Grid (LED gap)</option>
          <option value="glow">Soft Glow</option>
        </select>
        <span id="fps-counter" class="text-primary font-mono text-sm min-w-[80px] text-right">-- FPS</span>
      </div>

      <div id="canvas-wrap" class="relative mb-3">
        <canvas
          id="simulator-canvas"
          width="800"
          height="480"
          class="border border-base-content/10 rounded"
          style="image-rendering: pixelated;"
          phx-hook="Simulator"
          data-faces={Jason.encode!(Enum.map(@faces, &%{id: &1.id, name: &1.name, slug: &1.slug}))}
          data-selected={@selected_face_id}
        >
        </canvas>
        <div id="drop-overlay" class="absolute inset-0 hidden items-center justify-center bg-primary/15 border-2 border-dashed border-primary rounded text-lg text-primary pointer-events-none">
          Drop .wasm here
        </div>
      </div>

      <div class="flex gap-3 w-full max-w-[820px]">
        <div class="flex-1 bg-base-200 border border-base-content/10 rounded p-2 max-h-[200px] overflow-y-auto">
          <h3 class="text-xs text-base-content/40 uppercase tracking-wider mb-1">Console</h3>
          <pre id="console-output" class="text-xs whitespace-pre-wrap break-all text-base-content/60"></pre>
        </div>
        <div class="flex-1 bg-base-200 border border-base-content/10 rounded p-2 max-h-[200px] overflow-y-auto">
          <h3 class="text-xs text-base-content/40 uppercase tracking-wider mb-1">Stats</h3>
          <div id="stats-content" class="text-xs leading-relaxed"></div>
        </div>
      </div>
    </div>
    """
  end
end
