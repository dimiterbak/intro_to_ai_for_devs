using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace DotNetOpenAI;

/// <summary>
/// C# port of python/coding_agent_tools.py.
/// Provides simple filesystem and shell utilities for the coding agent.
/// </summary>
public class CodingAgentTools
{
    private readonly string _projectDir;

    // Directories to skip when building file tree or searching
    private static readonly HashSet<string> SkipDirs = new(StringComparer.OrdinalIgnoreCase)
    {
        ".venv",
        "__pycache__",
        ".git",
        ".pytest_cache",
        ".mypy_cache",
        ".coverage",
        "node_modules",
        ".DS_Store",
        "bin",
        "obj"
    };

    public CodingAgentTools(string projectDir)
    {
        _projectDir = Path.GetFullPath(projectDir);
    }

    public string ReadFile(string filepath)
    {
        var absPath = Path.GetFullPath(Path.Combine(_projectDir, filepath));
        // Prevent path escape outside project directory
        if (!absPath.StartsWith(_projectDir, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Access outside project directory is not allowed.");
        }
        return File.ReadAllText(absPath, Encoding.UTF8);
    }

    public string WriteFile(string filepath, string content)
    {
        var absPath = Path.GetFullPath(Path.Combine(_projectDir, filepath));
        if (!absPath.StartsWith(_projectDir, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Access outside project directory is not allowed.");
        }
        Directory.CreateDirectory(Path.GetDirectoryName(absPath)!);
        File.WriteAllText(absPath, content, Encoding.UTF8);
        return $"Successfully wrote to {filepath}";
    }

    public string SeeFileTree(string rootDir = ".")
    {
        var absRoot = Path.GetFullPath(Path.Combine(_projectDir, rootDir));
        if (!absRoot.StartsWith(_projectDir, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Access outside project directory is not allowed.");
        }

        var results = new List<string>();
        var rootUri = new Uri(_projectDir.EndsWith(Path.DirectorySeparatorChar) ? _projectDir : _projectDir + Path.DirectorySeparatorChar);

        foreach (var dir in Directory.EnumerateDirectories(absRoot, "*", SearchOption.AllDirectories))
        {
            var name = Path.GetFileName(dir);
            if (SkipDirs.Contains(name))
            {
                // skip walking into this dir by skipping files under it in the next loop
                continue;
            }
            var rel = rootUri.MakeRelativeUri(new Uri(dir + Path.DirectorySeparatorChar)).ToString();
            results.Add(Uri.UnescapeDataString(rel.TrimEnd('/')));
        }

        foreach (var file in Directory.EnumerateFiles(absRoot, "*", SearchOption.AllDirectories))
        {
            try
            {
                if (IsUnderSkippedDirectory(file))
                {
                    continue;
                }
                var rel = rootUri.MakeRelativeUri(new Uri(file)).ToString();
                results.Add(Uri.UnescapeDataString(rel));
            }
            catch { /* ignore */ }
        }

        return JsonSerializer.Serialize(results, new JsonSerializerOptions { WriteIndented = true });
    }

    public string ExecuteBashCommand(string command, string? cwd = null)
    {
        // Block running Django development server, matching python version's guard
        if (command.Contains("runserver", StringComparison.OrdinalIgnoreCase))
        {
            return JsonSerializer.Serialize(new
            {
                stdout = "",
                stderr = "Error: Running the Django development server (runserver) is not allowed through this tool.",
                returncode = 1
            }, new JsonSerializerOptions { WriteIndented = true });
        }

        var absCwd = Path.GetFullPath(Path.Combine(_projectDir, string.IsNullOrWhiteSpace(cwd) ? "." : cwd));
        if (!absCwd.StartsWith(_projectDir, StringComparison.Ordinal))
        {
            absCwd = _projectDir; // fallback to project root
        }

        var startInfo = new ProcessStartInfo
        {
            FileName = "/bin/bash",
            ArgumentList = { "-lc", command },
            WorkingDirectory = absCwd,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8
        };

        string stdout, stderr;
        int exitCode;
        try
        {
            using var proc = new Process { StartInfo = startInfo };
            proc.Start();

            // Read streams fully, then wait with timeout
            stdout = proc.StandardOutput.ReadToEnd();
            stderr = proc.StandardError.ReadToEnd();

            bool exited = proc.WaitForExit(15_000); // 15 seconds
            if (!exited)
            {
                try { proc.Kill(entireProcessTree: true); } catch { }
                exitCode = 124;
                if (!string.IsNullOrEmpty(stderr)) stderr += "\n";
                stderr += "Timeout after 15 seconds.";
            }
            else
            {
                exitCode = proc.ExitCode;
            }
        }
        catch (Exception ex)
        {
            stdout = string.Empty;
            stderr = ex.Message;
            exitCode = 1;
        }

        return JsonSerializer.Serialize(new { stdout, stderr, returncode = exitCode }, new JsonSerializerOptions { WriteIndented = true });
    }

    public string SearchInFiles(string pattern, string rootDir = ".")
    {
        var absRoot = Path.GetFullPath(Path.Combine(_projectDir, rootDir));
        if (!absRoot.StartsWith(_projectDir, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Access outside project directory is not allowed.");
        }

        var matches = new List<object>();
        foreach (var file in Directory.EnumerateFiles(absRoot, "*", SearchOption.AllDirectories))
        {
            try
            {
                if (IsUnderSkippedDirectory(file))
                {
                    continue;
                }

                // Attempt to read as UTF-8; if it fails, skip
                using var fs = File.OpenRead(file);
                using var sr = new StreamReader(fs, Encoding.UTF8, detectEncodingFromByteOrderMarks: true, bufferSize: 4096, leaveOpen: false);
                string? line;
                int lineNumber = 0;
                while ((line = sr.ReadLine()) != null)
                {
                    lineNumber++;
                    if (line.Contains(pattern, StringComparison.Ordinal))
                    {
                        var rel = Path.GetRelativePath(_projectDir, file);
                        matches.Add(new { path = rel, line = lineNumber, content = line.Trim() });
                    }
                }
            }
            catch
            {
                // Ignore unreadable/binary files
            }
        }

        return JsonSerializer.Serialize(matches, new JsonSerializerOptions { WriteIndented = true });
    }

    private static bool IsUnderSkippedDirectory(string fullPath)
    {
        try
        {
            var segments = fullPath.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            foreach (var seg in segments)
            {
                if (SkipDirs.Contains(seg)) return true;
            }
        }
        catch { }
        return false;
    }
}
