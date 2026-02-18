defmodule Studio.Faces do
  import Ecto.Query
  alias Studio.Repo
  alias Studio.Faces.{Face, SourceFile}

  @listing_fields [:id, :name, :slug, :language, :wasm_size, :compiled_at, :published, :inserted_at, :updated_at]

  def list_faces do
    Face
    |> select([f], struct(f, ^@listing_fields))
    |> order_by(:name)
    |> Repo.all()
  end

  def list_published_faces do
    Face
    |> where([f], f.published == true and not is_nil(f.wasm_binary))
    |> select([f], struct(f, ^@listing_fields))
    |> order_by(:name)
    |> Repo.all()
  end

  def get_face!(id), do: Repo.get!(Face, id)

  def get_face_by_slug(slug), do: Repo.get_by(Face, slug: slug)

  def get_face_with_sources!(id) do
    Face
    |> Repo.get!(id)
    |> Repo.preload(source_files: from(s in SourceFile, order_by: s.position))
  end

  def create_face(attrs) do
    %Face{}
    |> Face.changeset(attrs)
    |> Repo.insert()
  end

  def update_face(%Face{} = face, attrs) do
    face
    |> Face.changeset(attrs)
    |> Repo.update()
  end

  def delete_face(%Face{} = face) do
    Repo.delete(face)
  end

  def toggle_published(%Face{} = face) do
    update_face(face, %{published: !face.published})
  end

  # Source files

  def get_source_file!(id), do: Repo.get!(SourceFile, id)

  def create_source_file(attrs) do
    %SourceFile{}
    |> SourceFile.changeset(attrs)
    |> Repo.insert()
  end

  def update_source_file(%SourceFile{} = sf, attrs) do
    sf
    |> SourceFile.changeset(attrs)
    |> Repo.update()
  end

  def delete_source_file(%SourceFile{} = sf) do
    Repo.delete(sf)
  end

  def list_source_files(face_id) do
    SourceFile
    |> where([s], s.face_id == ^face_id)
    |> order_by(:position)
    |> Repo.all()
  end

  # WASM binary management

  def store_wasm(%Face{} = face, wasm_binary) when is_binary(wasm_binary) do
    update_face(face, %{
      wasm_binary: wasm_binary,
      wasm_size: byte_size(wasm_binary),
      compiled_at: DateTime.utc_now() |> DateTime.truncate(:second)
    })
  end

  def generate_index_txt do
    list_published_faces()
    |> Enum.map(fn face -> "#{face.name}|#{face.slug}.wasm" end)
    |> Enum.join("\n")
  end
end
