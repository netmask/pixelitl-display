defmodule Studio.Repo.Migrations.CreateSourceFiles do
  use Ecto.Migration

  def change do
    create table(:source_files) do
      add :face_id, references(:faces, on_delete: :delete_all), null: false
      add :filename, :string, null: false
      add :content, :text, default: ""
      add :position, :integer, default: 0

      timestamps()
    end

    create index(:source_files, [:face_id])
    create unique_index(:source_files, [:face_id, :filename])
  end
end
