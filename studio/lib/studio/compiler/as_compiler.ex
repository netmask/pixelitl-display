defmodule Studio.Compiler.AsCompiler do
  @sdk_path Path.expand("../../../../sdk", __DIR__)

  def compile(source_files, _output_path) do
    tmp_dir = System.tmp_dir!()
    work_dir = Path.join(tmp_dir, "pixelitl_as_#{:erlang.unique_integer([:positive])}")
    File.mkdir_p!(work_dir)

    try do
      # Copy the SDK TypeScript bindings
      sdk_ts = Path.join(@sdk_path, "pixelitl.ts")

      unless File.exists?(sdk_ts) do
        raise "pixelitl.ts SDK not found at #{sdk_ts}"
      end

      File.cp!(sdk_ts, Path.join(work_dir, "pixelitl.ts"))

      # Write source files to work dir
      for sf <- source_files do
        File.write!(Path.join(work_dir, sf.filename), sf.content)
      end

      # Find the main .ts file (not pixelitl.ts)
      main_file =
        Enum.find(source_files, fn sf ->
          String.ends_with?(sf.filename, ".ts")
        end)

      if is_nil(main_file) do
        {:error, "No .ts source file found", []}
      else
        wasm_out = Path.join(work_dir, "output.wasm")

        args = [
          main_file.filename,
          "-o", wasm_out,
          "--optimize",
          "--runtime", "stub",
          "--use", "abort=",
          "--initialMemory", "2",
          "--maximumMemory", "4",
          "--exportStart", "_start"
        ]

        asc_path = find_asc()

        case System.cmd(asc_path, args, cd: work_dir, stderr_to_stdout: true) do
          {_output, 0} ->
            wasm_binary = File.read!(wasm_out)
            {:ok, wasm_binary}

          {output, _code} ->
            errors = parse_errors(output, main_file.filename)
            {:error, output, errors}
        end
      end
    rescue
      e -> {:error, Exception.message(e), []}
    after
      File.rm_rf!(work_dir)
    end
  end

  defp find_asc do
    # Check common locations for asc
    candidates = [
      System.find_executable("asc"),
      Path.expand("~/.npm-global/bin/asc"),
      "/opt/homebrew/bin/asc",
      "/usr/local/bin/asc"
    ]

    case Enum.find(candidates, &(&1 && File.exists?(&1))) do
      nil -> raise "asc (AssemblyScript compiler) not found. Install with: npm install -g assemblyscript"
      path -> path
    end
  end

  defp parse_errors(output, _main_file) do
    # AssemblyScript error format: ERROR TS2304: message
    #  in filename(line,col)
    # or: ERROR AS200: message
    #  in filename(line,col)
    lines = String.split(output, "\n")
    parse_error_lines(lines, [])
  end

  defp parse_error_lines([], acc), do: Enum.reverse(acc)

  defp parse_error_lines([line | rest], acc) do
    case Regex.run(~r/^(ERROR|WARNING)\s+\w+:\s+(.+)$/, line) do
      [_, severity_str, message] ->
        severity = if severity_str == "ERROR", do: "error", else: "warning"

        # Next line should have location: " in filename(line,col)"
        case rest do
          [loc_line | rest2] ->
            case Regex.run(~r/in\s+(.+?)\((\d+),(\d+)\)/, loc_line) do
              [_, file, line_num, col] ->
                error = %{
                  file: Path.basename(file),
                  line: String.to_integer(line_num),
                  column: String.to_integer(col),
                  severity: severity,
                  message: message
                }

                parse_error_lines(rest2, [error | acc])

              _ ->
                error = %{
                  file: "main.ts",
                  line: 1,
                  column: 1,
                  severity: severity,
                  message: message
                }

                parse_error_lines(rest, [error | acc])
            end

          [] ->
            error = %{
              file: "main.ts",
              line: 1,
              column: 1,
              severity: severity,
              message: message
            }

            parse_error_lines(rest, [error | acc])
        end

      _ ->
        parse_error_lines(rest, acc)
    end
  end
end
