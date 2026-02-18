defmodule Studio.Faces.Face do
  use Ecto.Schema
  import Ecto.Changeset

  schema "faces" do
    field :name, :string
    field :slug, :string
    field :language, :string, default: "c"
    field :wasm_binary, :binary
    field :wasm_size, :integer, default: 0
    field :compiled_at, :utc_datetime
    field :published, :boolean, default: true

    has_many :source_files, Studio.Faces.SourceFile

    timestamps()
  end

  def changeset(face, attrs) do
    face
    |> cast(attrs, [:name, :slug, :language, :wasm_binary, :wasm_size, :compiled_at, :published])
    |> validate_required([:name, :slug, :language])
    |> unique_constraint(:slug)
    |> maybe_set_slug()
  end

  defp maybe_set_slug(changeset) do
    case get_field(changeset, :slug) do
      nil ->
        name = get_field(changeset, :name) || ""
        slug = name |> String.downcase() |> String.replace(~r/[^a-z0-9]+/, "_") |> String.trim("_")
        put_change(changeset, :slug, slug)
      _ ->
        changeset
    end
  end
end
