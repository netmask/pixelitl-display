defmodule StudioWeb.EditorLive do
  use StudioWeb, :live_view

  alias Studio.Faces
  alias Studio.Compiler

  @impl true
  def mount(params, _session, socket) do
    faces = Faces.list_faces()

    socket =
      case params do
        %{"id" => id} ->
          face = Faces.get_face_with_sources!(id)
          Phoenix.PubSub.subscribe(Studio.PubSub, "compiler:#{face.id}")

          active_file =
            case face.source_files do
              [first | _] -> first
              [] -> nil
            end

          socket
          |> assign(
            face: face,
            faces: faces,
            source_files: face.source_files,
            active_file: active_file,
            compile_output: nil,
            compile_errors: [],
            compiling: false,
            page_title: "Edit: #{face.name}"
          )

        _ ->
          socket
          |> assign(
            face: nil,
            faces: faces,
            source_files: [],
            active_file: nil,
            compile_output: nil,
            compile_errors: [],
            compiling: false,
            page_title: "Editor"
          )
      end

    {:ok, socket}
  end

  @impl true
  def handle_event("select_face", %{"id" => id}, socket) do
    {:noreply, push_navigate(socket, to: ~p"/editor/#{id}")}
  end

  def handle_event("select_file", %{"id" => id}, socket) do
    file = Enum.find(socket.assigns.source_files, &(&1.id == String.to_integer(id)))
    {:noreply, assign(socket, active_file: file)}
  end

  def handle_event("save_content", %{"content" => content}, socket) do
    case socket.assigns.active_file do
      nil ->
        {:noreply, socket}

      file ->
        {:ok, updated} = Faces.update_source_file(file, %{content: content})

        source_files =
          Enum.map(socket.assigns.source_files, fn sf ->
            if sf.id == updated.id, do: updated, else: sf
          end)

        {:noreply, assign(socket, active_file: updated, source_files: source_files)}
    end
  end

  def handle_event("compile", _params, socket) do
    case socket.assigns.face do
      nil ->
        {:noreply, socket}

      face ->
        Compiler.compile(face.id)
        {:noreply, assign(socket, compiling: true, compile_output: nil, compile_errors: [])}
    end
  end

  def handle_event("new_face", %{"name" => name, "language" => language}, socket) do
    slug = name |> String.downcase() |> String.replace(~r/[^a-z0-9]+/, "_") |> String.trim("_")

    case Faces.create_face(%{name: name, slug: slug, language: language}) do
      {:ok, face} ->
        {filename, template} =
          case language do
            "assemblyscript" -> {"#{slug}.ts", as_template(name)}
            _ -> {"#{slug}.c", c_template(name)}
          end

        {:ok, _sf} =
          Faces.create_source_file(%{
            face_id: face.id,
            filename: filename,
            content: template,
            position: 0
          })

        {:noreply, push_navigate(socket, to: ~p"/editor/#{face.id}")}

      {:error, _changeset} ->
        {:noreply, put_flash(socket, :error, "Could not create face")}
    end
  end

  def handle_event("new_file", %{"filename" => filename}, socket) do
    case socket.assigns.face do
      nil ->
        {:noreply, socket}

      face ->
        pos = length(socket.assigns.source_files)

        case Faces.create_source_file(%{
               face_id: face.id,
               filename: filename,
               content: "",
               position: pos
             }) do
          {:ok, sf} ->
            source_files = socket.assigns.source_files ++ [sf]
            {:noreply, assign(socket, source_files: source_files, active_file: sf)}

          {:error, _} ->
            {:noreply, put_flash(socket, :error, "Could not create file")}
        end
    end
  end

  def handle_event("delete_file", %{"id" => id}, socket) do
    sf = Faces.get_source_file!(id)
    {:ok, _} = Faces.delete_source_file(sf)

    source_files = Enum.reject(socket.assigns.source_files, &(&1.id == sf.id))

    active_file =
      if socket.assigns.active_file && socket.assigns.active_file.id == sf.id do
        List.first(source_files)
      else
        socket.assigns.active_file
      end

    {:noreply, assign(socket, source_files: source_files, active_file: active_file)}
  end

  @impl true
  def handle_info({Studio.Compiler, :compiling}, socket) do
    {:noreply, assign(socket, compiling: true)}
  end

  def handle_info({Studio.Compiler, {:compiled, size, face}}, socket) do
    {:noreply,
     socket
     |> assign(compiling: false, compile_output: "Compiled OK (#{size} bytes)", compile_errors: [], face: face)
     |> push_event("wasm_compiled", %{face_id: face.id, slug: face.slug})}
  end

  def handle_info({Studio.Compiler, {:compile_error, output, errors}}, socket) do
    {:noreply,
     socket
     |> assign(compiling: false, compile_output: output, compile_errors: errors)
     |> push_event("compile_errors", %{errors: errors})}
  end

  defp c_template(name) do
    """
    #include "pixelitl.h"

    static float t;

    __attribute__((export_name("app_init")))
    void app_init(void) {
        t = 0.0f;
    }

    __attribute__((export_name("app_update")))
    void app_update(int frame, int dt) {
        t += (float)dt * 0.001f;
    }

    __attribute__((export_name("app_draw")))
    void app_draw(void) {
        clear(0);
        int w = width();
        int h = height();
        // #{name}: draw here
        unsigned int color = hsv(t * 60.0f, 1.0f, 1.0f);
        draw_str(2, 2, "#{name}", color);
    }
    """
  end

  @impl true
  def render(assigns) do
    ~H"""
    <div class="flex h-screen bg-base-300" id="editor-page">
      <%!-- Left sidebar: faces + files --%>
      <div class="w-48 bg-base-200 border-r border-base-content/10 flex flex-col shrink-0">
        <div class="p-2 border-b border-base-content/10">
          <h2 class="font-bold text-xs uppercase tracking-wider text-base-content/50 mb-2">Faces</h2>
          <div class="flex flex-col gap-0.5">
            <button
              :for={f <- @faces}
              class={[
                "btn btn-xs justify-start font-normal",
                if(@face && @face.id == f.id, do: "btn-primary", else: "btn-ghost")
              ]}
              phx-click="select_face"
              phx-value-id={f.id}
            >
              {f.name}
            </button>
          </div>
        </div>

        <div class="p-2 border-b border-base-content/10">
          <h3 class="text-xs text-base-content/50 uppercase tracking-wider mb-1">New Face</h3>
          <form phx-submit="new_face" class="flex flex-col gap-1">
            <input type="text" name="name" placeholder="Name" required class="input input-xs input-bordered w-full" />
            <select name="language" class="select select-xs select-bordered w-full">
              <option value="c">C</option>
              <option value="assemblyscript">AssemblyScript</option>
            </select>
            <button type="submit" class="btn btn-xs btn-primary w-full">Create</button>
          </form>
        </div>

        <div :if={@face} class="p-2 flex-1 overflow-y-auto">
          <h3 class="text-xs text-base-content/50 uppercase tracking-wider mb-1">Files</h3>
          <div class="flex flex-col gap-0.5">
            <div
              :for={sf <- @source_files}
              class={[
                "flex items-center justify-between px-2 py-1 rounded text-xs cursor-pointer group",
                if(@active_file && @active_file.id == sf.id, do: "bg-primary text-primary-content", else: "hover:bg-base-300")
              ]}
            >
              <span phx-click="select_file" phx-value-id={sf.id} class="flex-1 truncate font-mono">
                {sf.filename}
              </span>
              <button
                phx-click="delete_file"
                phx-value-id={sf.id}
                data-confirm="Delete this file?"
                class="opacity-0 group-hover:opacity-60 hover:!opacity-100 ml-1"
              >
                <.icon name="hero-x-mark" class="size-3" />
              </button>
            </div>
          </div>
          <form phx-submit="new_file" class="mt-2">
            <input type="text" name="filename" placeholder="new_file.c" class="input input-xs input-bordered w-full" />
          </form>
        </div>
      </div>

      <%!-- Center: code editor + output --%>
      <div class="flex-1 flex flex-col min-w-0">
        <%!-- Toolbar --%>
        <div class="h-9 bg-base-200 border-b border-base-content/10 flex items-center px-3 gap-2 shrink-0">
          <%!-- File tabs --%>
          <div :if={@face} class="flex gap-0.5 overflow-x-auto">
            <button
              :for={sf <- @source_files}
              class={[
                "px-3 py-1 text-xs rounded-t font-mono whitespace-nowrap",
                if(@active_file && @active_file.id == sf.id,
                  do: "bg-base-300 text-base-content",
                  else: "text-base-content/50 hover:text-base-content/80")
              ]}
              phx-click="select_file"
              phx-value-id={sf.id}
            >
              {sf.filename}
            </button>
          </div>
          <div class="flex-1" />
          <span :if={@face} class="text-xs text-base-content/40 font-mono">{@face.name}</span>
          <button
            :if={@face}
            phx-click="compile"
            disabled={@compiling}
            class={["btn btn-xs btn-primary gap-1", if(@compiling, do: "loading")]}
          >
            <.icon :if={!@compiling} name="hero-play" class="size-3" />
            {if @compiling, do: "Compiling...", else: "Build & Run"}
          </button>
          <.link navigate={~p"/simulator"} class="btn btn-xs btn-ghost">Simulator</.link>
          <.link navigate={~p"/faces"} class="btn btn-xs btn-ghost">Faces</.link>
        </div>

        <%!-- Editor + output vertical split --%>
        <div class="flex-1 flex flex-col min-h-0">
          <div
            id="monaco-editor"
            phx-hook="Monaco"
            phx-update="ignore"
            class="flex-1 min-h-0"
            data-language={if(@active_file, do: language_for(@active_file.filename), else: "c")}
            data-content={if(@active_file, do: @active_file.content, else: "")}
            data-file-id={if(@active_file, do: @active_file.id, else: "")}
          >
          </div>

          <%!-- Output panel --%>
          <div class="h-28 bg-base-200 border-t border-base-content/10 p-2 overflow-y-auto shrink-0">
            <h3 class="text-xs text-base-content/40 uppercase tracking-wider mb-1">Output</h3>
            <pre class={[
              "text-xs whitespace-pre-wrap font-mono",
              if(@compile_errors != [], do: "text-error", else: "text-success")
            ]}>{@compile_output || "Ready. Press Ctrl+S or Build & Run to compile."}</pre>
          </div>
        </div>
      </div>

      <%!-- Right: live simulator preview --%>
      <div :if={@face} class="w-[440px] border-l border-base-content/10 flex flex-col bg-base-300 shrink-0">
        <%!-- Simulator toolbar --%>
        <div class="h-9 bg-base-200 border-b border-base-content/10 flex items-center px-3 gap-2 shrink-0">
          <span class="text-xs text-base-content/50 uppercase tracking-wider font-bold">Preview</span>
          <div class="flex-1" />
          <select id="editor-glow-mode" class="select select-xs select-bordered">
            <option value="flat">Flat</option>
            <option value="grid" selected>Grid</option>
            <option value="glow">Glow</option>
          </select>
          <span id="editor-fps" class="text-xs text-primary font-mono min-w-[50px] text-right">-- FPS</span>
        </div>

        <%!-- Canvas --%>
        <div class="flex items-center justify-center p-3">
          <canvas
            id="mini-simulator"
            width="800"
            height="480"
            class="w-full rounded border border-base-content/10"
            style="image-rendering: pixelated;"
            phx-hook="MiniSimulator"
            data-face-slug={@face.slug}
          >
          </canvas>
        </div>

        <%!-- Console + Stats --%>
        <div class="flex-1 flex flex-col min-h-0 px-3 pb-3 gap-2">
          <div class="flex-1 bg-base-200 border border-base-content/10 rounded p-2 overflow-y-auto min-h-0">
            <h3 class="text-xs text-base-content/40 uppercase tracking-wider mb-1">Console</h3>
            <pre id="editor-console" class="text-xs whitespace-pre-wrap break-all text-base-content/60 font-mono"></pre>
          </div>
          <div class="h-[120px] bg-base-200 border border-base-content/10 rounded p-2 overflow-y-auto shrink-0">
            <h3 class="text-xs text-base-content/40 uppercase tracking-wider mb-1">Stats</h3>
            <div id="editor-stats" class="text-xs leading-relaxed font-mono"></div>
          </div>
        </div>
      </div>

      <%!-- Empty state when no face selected --%>
      <div :if={!@face} class="flex-1 flex items-center justify-center">
        <div class="text-center text-base-content/30">
          <.icon name="hero-code-bracket" class="size-16 mx-auto mb-4" />
          <p class="text-lg">Select a face to start editing</p>
          <p class="text-sm mt-1">or create a new one from the sidebar</p>
        </div>
      </div>
    </div>
    """
  end

  defp as_template(name) do
    """
    import {
      clear, set_pixel, fill_rect, draw_rect, draw_circle, fill_circle,
      draw_line_h, draw_line_v, draw_str, draw_str_big,
      rgb, hsv, dim_color, lerp_color,
      sinf, cosf, sqrtf, rand,
      width, height, get_frame, millis, log
    } from "./pixelitl";

    let t: f32 = 0;

    export function app_init(): void {
      t = 0;
    }

    export function app_update(frame: i32, dt: i32): void {
      t += <f32>dt * 0.001;
    }

    export function app_draw(): void {
      clear(0);
      const w = width();
      const h = height();
      // #{name}: draw here
      const color = hsv(t * 60.0, 1.0, 1.0);
      draw_str(2, 2, "#{name}", color);
    }
    """
  end

  defp language_for(filename) do
    cond do
      String.ends_with?(filename, ".c") -> "c"
      String.ends_with?(filename, ".h") -> "c"
      String.ends_with?(filename, ".rs") -> "rust"
      String.ends_with?(filename, ".ts") -> "typescript"
      true -> "plaintext"
    end
  end
end
