defmodule Studio.Compiler.CCompiler do
  @clang "/opt/homebrew/opt/llvm/bin/clang"
  @sdk_path Path.expand("../../../../sdk", __DIR__)

  def compile(source_files, output_path) do
    tmp_dir = System.tmp_dir!()
    work_dir = Path.join(tmp_dir, "pixelitl_compile_#{:erlang.unique_integer([:positive])}")
    File.mkdir_p!(work_dir)

    try do
      # Write source files to work dir
      for sf <- source_files do
        File.write!(Path.join(work_dir, sf.filename), sf.content)
      end

      # Find the main .c file
      main_file =
        Enum.find(source_files, fn sf -> String.ends_with?(sf.filename, ".c") end)

      if is_nil(main_file) do
        {:error, "No .c source file found"}
      else
        wasm_out = Path.join(work_dir, "output.wasm")

        args = [
          "--target=wasm32",
          "-O2",
          "-nostdlib",
          "-fno-builtin",
          "-Wl,--no-entry",
          "-Wl,--export=app_init",
          "-Wl,--export=app_update",
          "-Wl,--export=app_draw",
          "-Wl,--allow-undefined",
          "-Wl,--initial-memory=131072",
          "-Wl,--max-memory=262144",
          "-I#{@sdk_path}",
          "-o", wasm_out,
          main_file.filename
        ]

        case System.cmd(@clang, args, cd: work_dir, stderr_to_stdout: true) do
          {_output, 0} ->
            wasm_binary = File.read!(wasm_out)

            if output_path do
              File.mkdir_p!(Path.dirname(output_path))
              File.write!(output_path, wasm_binary)
            end

            {:ok, wasm_binary}

          {output, _code} ->
            errors = parse_errors(output, main_file.filename)
            {:error, output, errors}
        end
      end
    after
      File.rm_rf!(work_dir)
    end
  end

  defp parse_errors(output, _main_file) do
    output
    |> String.split("\n")
    |> Enum.flat_map(fn line ->
      case Regex.run(~r/^(.+?):(\d+):(\d+):\s+(error|warning):\s+(.+)$/, line) do
        [_, file, line_num, col, severity, message] ->
          [%{
            file: Path.basename(file),
            line: String.to_integer(line_num),
            column: String.to_integer(col),
            severity: severity,
            message: message
          }]
        _ ->
          []
      end
    end)
  end
end
