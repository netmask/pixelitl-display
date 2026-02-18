defmodule Studio.Repo.Migrations.CreateFaces do
  use Ecto.Migration

  def change do
    create table(:faces) do
      add :name, :string, null: false
      add :slug, :string, null: false
      add :language, :string, null: false, default: "c"
      add :wasm_binary, :binary
      add :wasm_size, :integer, default: 0
      add :compiled_at, :utc_datetime
      add :published, :boolean, default: true

      timestamps()
    end

    create unique_index(:faces, [:slug])
  end
end
