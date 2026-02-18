defmodule Studio.Compiler do
  use GenServer
  alias Studio.Faces
  alias Studio.Compiler.CCompiler
  alias Studio.Compiler.AsCompiler

  def start_link(_opts) do
    GenServer.start_link(__MODULE__, %{}, name: __MODULE__)
  end

  def compile(face_id) do
    GenServer.cast(__MODULE__, {:compile, face_id})
  end

  # GenServer callbacks

  @impl true
  def init(state) do
    {:ok, state}
  end

  @impl true
  def handle_cast({:compile, face_id}, state) do
    Task.start(fn -> do_compile(face_id) end)
    {:noreply, state}
  end

  defp do_compile(face_id) do
    broadcast(face_id, :compiling)

    face = Faces.get_face_with_sources!(face_id)
    source_files = face.source_files

    result =
      case face.language do
        "c" -> CCompiler.compile(source_files, nil)
        "assemblyscript" -> AsCompiler.compile(source_files, nil)
        _ -> {:error, "Unsupported language: #{face.language}", []}
      end

    case result do
      {:ok, wasm_binary} ->
        {:ok, updated_face} = Faces.store_wasm(face, wasm_binary)
        broadcast(face_id, {:compiled, byte_size(wasm_binary), updated_face})

      {:error, output, errors} ->
        broadcast(face_id, {:compile_error, output, errors})

      {:error, message} ->
        broadcast(face_id, {:compile_error, message, []})
    end
  end

  defp broadcast(face_id, message) do
    Phoenix.PubSub.broadcast(Studio.PubSub, "compiler:#{face_id}", {__MODULE__, message})
  end
end
