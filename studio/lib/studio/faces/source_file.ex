defmodule Studio.Faces.SourceFile do
  use Ecto.Schema
  import Ecto.Changeset

  schema "source_files" do
    field :filename, :string
    field :content, :string, default: ""
    field :position, :integer, default: 0

    belongs_to :face, Studio.Faces.Face

    timestamps()
  end

  def changeset(source_file, attrs) do
    source_file
    |> cast(attrs, [:filename, :content, :position, :face_id])
    |> validate_required([:filename, :face_id])
    |> unique_constraint([:face_id, :filename])
  end
end
