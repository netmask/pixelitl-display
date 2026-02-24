# Seed faces from sdk/examples/
alias Studio.Repo
alias Studio.Faces.{Face, SourceFile}

sdk_examples = Path.expand("../../../sdk/examples", __DIR__)

faces = [
  {"Plasma", "plasma", "c"},
  {"Clock", "clock", "c"},
  {"Demo", "demo", "c"},
  {"HD Demo", "hd_demo", "c"},
  {"Ultra Demo", "ultra_demo", "c"},
  {"Weather", "weather", "c"},
  {"Buttons", "buttons", "c"},
  {"Store", "store", "c"},
  {"Pong", "pong", "assemblyscript"},
  {"Holi", "holi", "c"},
  {"Nyan Cat", "nyan", "c"},
  {"Glucose", "glucose", "c"},
]

for {name, slug, language} <- faces do
  wasm_path = Path.join(sdk_examples, "#{slug}.wasm")

  source_ext = case language do
    "assemblyscript" -> ".ts"
    _ -> ".c"
  end

  source_path = Path.join(sdk_examples, "#{slug}#{source_ext}")

  wasm_binary =
    if File.exists?(wasm_path) do
      File.read!(wasm_path)
    else
      nil
    end

  source_content =
    if File.exists?(source_path) do
      File.read!(source_path)
    else
      ""
    end

  face_attrs = %{
    name: name,
    slug: slug,
    language: language,
    wasm_binary: wasm_binary,
    wasm_size: if(wasm_binary, do: byte_size(wasm_binary), else: 0),
    compiled_at: if(wasm_binary, do: DateTime.utc_now() |> DateTime.truncate(:second), else: nil),
    published: wasm_binary != nil
  }

  # Upsert: delete existing face with same slug before inserting
  case Repo.get_by(Face, slug: slug) do
    nil -> :ok
    existing -> Repo.delete(existing)
  end

  {:ok, face} =
    %Face{}
    |> Face.changeset(face_attrs)
    |> Repo.insert()

  if source_content != "" do
    %SourceFile{}
    |> SourceFile.changeset(%{
      face_id: face.id,
      filename: "#{slug}#{source_ext}",
      content: source_content,
      position: 0
    })
    |> Repo.insert!()
  end

  IO.puts("Seeded: #{name} (#{slug})")
end
