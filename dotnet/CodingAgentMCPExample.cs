using OpenAI;
using OpenAI.Chat;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace DotNetOpenAI;

/// <summary>
/// A Coding Agent using tools (function calling) and model context protocol (MCP) service.
/// It exposes filesystem and shell tools and lets the model call them iteratively
/// until a final answer is produced.
/// </summary>
public static class CodingAgentMCPExample
{
    private static string? Endpoint => Environment.GetEnvironmentVariable("AI_ENDPOINT");
    private static string? DeploymentName => Environment.GetEnvironmentVariable("DEPLOYMENT_NAME");
    private static string? ApiKey => Environment.GetEnvironmentVariable("AI_API_KEY");

    public static async Task RunAsync()
    {
        if (string.IsNullOrWhiteSpace(Endpoint) || string.IsNullOrWhiteSpace(DeploymentName) || string.IsNullOrWhiteSpace(ApiKey))
        {
            Console.WriteLine("Error: Please set AI_ENDPOINT, DEPLOYMENT_NAME, and AI_API_KEY environment variables.");
            return;
        }

        ShowExamples();
        await InteractiveLoopAsync();
    }

    private static void ShowExamples()
    {
        Console.WriteLine(new string('=', 80));
        Console.WriteLine("CHATBOT EXAMPLES - Tool-Enabled Coding Agent (C#)");
        Console.WriteLine(new string('=', 80));

        string[] examples =
        {
            "Show me the file structure of this project",
            "Read the README.md file",
            "Search for any C# files that contain 'OpenAI'",
            "What states does Ohio share borders with?",
            "Create a simple hello world C# script in a new file called hello.cs"
        };

        for (int i = 0; i < examples.Length; i++)
        {
            Console.WriteLine($"\n--- Example {i + 1} ---\n{examples[i]}");
        }
    }

    private static OpenAIClient CreateClient() => new(new System.ClientModel.ApiKeyCredential(ApiKey!), new OpenAIClientOptions
    {
        Endpoint = new Uri(Endpoint!)
    });

    private sealed class ChatBot
    {
        private readonly List<ChatMessage> _messages = new();
        private readonly OpenAIClient _client;
        private readonly string _deployment;
        private readonly CodingAgentTools _tools;
        private readonly string _system;
        private int _promptCount = 0;

        // Define Chat tools (function calling) compatible with OpenAI .NET SDK
        private readonly ChatTool _readFileTool;
        private readonly ChatTool _writeFileTool;
        private readonly ChatTool _seeFileTreeTool;
        private readonly ChatTool _executeBashTool;
        private readonly ChatTool _searchInFilesTool;
    private readonly ChatTool _sequentialThinkingTool;

        public ChatBot(OpenAIClient client, string deployment, string? system, string projectPath)
        {
            _client = client;
            _deployment = deployment;
            _tools = new CodingAgentTools(projectPath);
            // Default system prompt with guardrails for when to use tools
            string defaultSystem =
                "You are a helpful coding assistant with access to file system tools. " +
                "Only use tools for file operations, code analysis, project navigation, or when the user explicitly asks you to run a shell command. " +
                "Do NOT use tools for general knowledge questions; answer from your own knowledge unless the user specifically references files, paths, or commands.";

            _system = string.IsNullOrWhiteSpace(system) ? defaultSystem : system!;
            _messages.Add(new SystemChatMessage(_system));

            _readFileTool = ChatTool.CreateFunctionTool(
                functionName: nameof(ReadFileFunc),
                functionDescription: "Read and return the contents of a file at the given relative filepath. Prefer this over running shell commands like 'cat'.",
                functionParameters: BinaryData.FromString("""
                {
                  "type": "object",
                  "properties": {
                    "filepath": {"type": "string", "description": "Path to the file, relative to the project directory"}
                  },
                  "required": ["filepath"]
                }
                """));

            _writeFileTool = ChatTool.CreateFunctionTool(
                functionName: nameof(WriteFileFunc),
                functionDescription: "Write content to a file at the given relative filepath, creating directories as needed",
                functionParameters: BinaryData.FromString("""
                {
                  "type": "object",
                  "properties": {
                    "filepath": {"type": "string", "description": "Path to the file, relative to the project directory"},
                    "content": {"type": "string", "description": "Content to write to the file"}
                  },
                  "required": ["filepath", "content"]
                }
                """));

            _seeFileTreeTool = ChatTool.CreateFunctionTool(
                functionName: nameof(SeeFileTreeFunc),
                functionDescription: "Return a list of all files and directories under the given root directory",
                functionParameters: BinaryData.FromString("""
                {
                  "type": "object",
                  "properties": {
                    "root_dir": {"type": "string", "description": "Root directory to list from, relative to the project directory"}
                  },
                  "required": []
                }
                """));

            _executeBashTool = ChatTool.CreateFunctionTool(
                functionName: nameof(ExecuteBashCommandFunc),
                functionDescription: "Execute a bash command in the shell and return its output, error, and exit code. Only use when the user explicitly asks to run a shell/terminal command. Prefer read_file/search_in_files for reading content.",
                functionParameters: BinaryData.FromString("""
                {
                  "type": "object",
                  "properties": {
                    "command": {"type": "string", "description": "The bash command to execute"},
                    "cwd": {"type": "string", "description": "Working directory to run the command in, relative to the project directory"}
                  },
                  "required": ["command"]
                }
                """));

            _searchInFilesTool = ChatTool.CreateFunctionTool(
                functionName: nameof(SearchInFilesFunc),
                functionDescription: "Search for a pattern in all files under the given root directory",
                functionParameters: BinaryData.FromString("""
                {
                  "type": "object",
                  "properties": {
                    "pattern": {"type": "string", "description": "Pattern to search for in files"},
                    "root_dir": {"type": "string", "description": "Root directory to search from, relative to the project directory"}
                  },
                  "required": ["pattern"]
                }
                """));

                        _sequentialThinkingTool = ChatTool.CreateFunctionTool(
                            functionName: "sequentialthinking",
                                functionDescription: "Call the sequentialthinking MCP server via a Node.js bridge. Allows step-by-step reasoning orchestration.",
                                functionParameters: BinaryData.FromString("""
                                {
                                    "type": "object",
                                    "properties": {
                                        "thought": {"type": "string", "description": "Your current thinking step."},
                                        "nextThoughtNeeded": {"type": "boolean", "description": "Whether another thought step is needed."},
                                        "thoughtNumber": {"type": "integer", "description": "Current thought number (1-based)."},
                                        "totalThoughts": {"type": "integer", "description": "Estimated total thoughts needed."},
                                        "isRevision": {"type": "boolean", "description": "Whether this revises previous thinking."},
                                        "revisesThought": {"type": "integer", "description": "Which thought is being reconsidered."},
                                        "branchFromThought": {"type": "integer", "description": "Branching point thought number."},
                                        "branchId": {"type": "string", "description": "Branch identifier."},
                                        "needsMoreThoughts": {"type": "boolean", "description": "If reaching end but realizing more thoughts are needed."}
                                    },
                                    "required": ["thought", "nextThoughtNeeded", "thoughtNumber", "totalThoughts"]
                                }
                                """));
        }

        public async Task<string> AskAsync(string userContent)
        {
            _messages.Add(new UserChatMessage(userContent));
            return await ExecuteAsync();
        }

        private ChatCompletionOptions BuildOptions(bool includeTools)
        {
            var options = new ChatCompletionOptions();
            if (includeTools)
            {
                options.Tools.Add(_readFileTool);
                options.Tools.Add(_writeFileTool);
                options.Tools.Add(_seeFileTreeTool);
                options.Tools.Add(_executeBashTool);
                options.Tools.Add(_searchInFilesTool);
                options.Tools.Add(_sequentialThinkingTool);
            }
            return options;
        }

        // Heuristic: expose tools only if the last user message suggests file/shell intent.
        // Examples that SHOULD enable tools: mentions of reading/writing/creating files, searching, grep,
        // file tree, directories/paths, opening files, running shell/CLI commands, common package managers,
        // programming file extensions, or path-like tokens.
        private bool ShouldEnableToolsForNextTurn()
        {
            // Find the last user message
            UserChatMessage? lastUser = null;
            for (int i = _messages.Count - 1; i >= 0; i--)
            {
                if (_messages[i] is UserChatMessage um)
                {
                    lastUser = um;
                    break;
                }
            }

            // If there's no user message yet, allow tools (safe default)
            if (lastUser is null)
            {
                return true;
            }

            var text = string.Join("\n", lastUser.Content.Select(c => c.Text ?? string.Empty)).ToLowerInvariant();
            string[] toolIntents = new[]
            {
                "read file", "write file", "create file", "append to file", "search", "grep", "file tree",
                "directory", "path", "open", "bash", "shell", "terminal", "run", "execute", "npm", "node",
                "ts-node", "tsc", "yarn", "pnpm", "pip", "python", "pytest", "make", "gradle", "cargo",
                "go run", "go build", ".ts", ".js", ".json", ".py", ".md", "/", "\\",
                // Explicit tool by name (normalize variants)
                "sequentialthinking", "sequential-thinking", "sequential_thinking"
            };
            return toolIntents.Any(w => text.Contains(w));
        }

        private async Task<string> ExecuteAsync()
        {
            var chatClient = _client.GetChatClient(_deployment);

            while (true)
            {
                _promptCount++;
                Console.WriteLine($"\n-- Prompt {_promptCount} --");
                Console.WriteLine("\n=== FULL PROMPT SENT TO MODEL ===");
                foreach (var msg in _messages)
                {
                    // Be defensive when printing content (some message types may not carry plain text parts)
                    string printed;
                    try
                    {
                        printed = string.Join("\n", msg.Content.Select(c => c.Text).Where(t => !string.IsNullOrEmpty(t)));
                    }
                    catch
                    {
                        printed = msg.ToString() ?? string.Empty;
                    }
                    Console.WriteLine($"  {printed}\n");
                }

                var includeTools = ShouldEnableToolsForNextTurn();
                var options = BuildOptions(includeTools);
                var completion = await chatClient.CompleteChatAsync(_messages, options);
                Console.WriteLine("=== END OF PROMPT ===\n");

                var result = completion.Value;

                if (result.FinishReason == ChatFinishReason.ToolCalls)
                {
                    // Add assistant message with tool calls to history
                    _messages.Add(new AssistantChatMessage(result));

                    // Execute all tool calls returned in this assistant message
                    foreach (var toolCall in result.ToolCalls)
                    {
                        Console.WriteLine($"🔧 Executing tool: {toolCall.FunctionName} with args: {toolCall.FunctionArguments}");
                        var toolResult = await ExecuteToolAsync(toolCall);
                        _messages.Add(new ToolChatMessage(toolCall.Id, toolResult));
                    }

                    // Loop continues: next iteration will print a new Prompt heade
                    continue;
                }
                else
                {
                    var text = result.Content.Count > 0 ? result.Content[0].Text : string.Empty;
                    _messages.Add(new AssistantChatMessage(result));
                    return text;
                }
            }
        }

        private async Task<string> ExecuteToolAsync(ChatToolCall toolCall)
        {
            try
            {
                // toolCall.FunctionArguments may be BinaryData; convert to string first
                var funcArgsStr = toolCall.FunctionArguments?.ToString() ?? "{}";
                using var argsJson = JsonDocument.Parse(funcArgsStr);
                var root = argsJson.RootElement;
                switch (toolCall.FunctionName)
                {
                    case nameof(ReadFileFunc):
                        return _tools.ReadFile(root.GetProperty("filepath").GetString()!);
                    case nameof(WriteFileFunc):
                        return _tools.WriteFile(
                            root.GetProperty("filepath").GetString()!,
                            root.GetProperty("content").GetString()!
                        );
                    case nameof(SeeFileTreeFunc):
                        {
                            string rootDir = root.TryGetProperty("root_dir", out var rd) ? rd.GetString() ?? "." : ".";
                            return _tools.SeeFileTree(rootDir);
                        }
                    case nameof(ExecuteBashCommandFunc):
                        {
                            // Gate bash execution: require explicit user intent to run shell/terminal commands
                            // Look at the last user message and check for indicative keywords
                            string lastUserText = string.Empty;
                            for (int i = _messages.Count - 1; i >= 0; i--)
                            {
                                if (_messages[i] is UserChatMessage um)
                                {
                                    lastUserText = string.Join("\n", um.Content.Select(c => c.Text)).ToLowerInvariant();
                                    break;
                                }
                            }

                            bool explicitRequest = false;
                            if (!string.IsNullOrEmpty(lastUserText))
                            {
                                // Match common signals that the user explicitly wants to run a command
                                // Equivalent to: (bash|shell|terminal|run|execute|npm|node|yarn|pnpm|pip|python|pytest|ts-node|tsc|make|gradle|cargo|go\s+run|go\s+build)
                                explicitRequest =
                                    Regex.IsMatch(lastUserText, @"\b(bash|shell|terminal|run|execute|npm|node|yarn|pnpm|pip|python|pytest|ts-node|tsc|make|gradle|cargo)\b")
                                    || Regex.IsMatch(lastUserText, @"go\s+run|go\s+build");
                            }

                            if (!explicitRequest)
                            {
                                var blocked = JsonSerializer.Serialize(new
                                {
                                    stdout = "",
                                    stderr = "Blocked: bash commands require explicit user request. Please proceed without execute_bash_command.",
                                    returncode = 1
                                }, new JsonSerializerOptions { WriteIndented = true });
                                return await Task.FromResult(blocked);
                            }

                            string command = root.GetProperty("command").GetString()!;
                            string? cwd = root.TryGetProperty("cwd", out var cwdEl) ? cwdEl.GetString() : null;
                            return await Task.FromResult(_tools.ExecuteBashCommand(command, cwd));
                        }
                    case nameof(SearchInFilesFunc):
                        {
                            string pattern = root.GetProperty("pattern").GetString()!;
                            string rootDir = root.TryGetProperty("root_dir", out var rd) ? rd.GetString() ?? "." : ".";
                            return _tools.SearchInFiles(pattern, rootDir);
                        }
                    case nameof(SequentialThinkingFunc):
                    case "sequentialthinking":
                        {
                            // Call Node bridge script using "node"; pass arguments as base64-encoded JSON via env.
                            // Prefer project-level node folder; script path is node/tools/mcp_seqthink_bridge.mjs relative to repo root.
                            var repoRoot = Directory.GetParent(Environment.CurrentDirectory)?.FullName ?? Environment.CurrentDirectory;
                            var bridgePath = Path.Combine(repoRoot, "node", "tools", "mcp_seqthink_bridge.mjs");
                            if (!File.Exists(bridgePath))
                            {
                                return $"Error: Bridge script not found at {bridgePath}.";
                            }

                            var argsDict = JsonSerializer.Deserialize<Dictionary<string, object>>(funcArgsStr) ?? new();
                            // Encode original args (not parsed root to preserve types) for the bridge
                            string argsB64 = Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(funcArgsStr));

                            var psi = new System.Diagnostics.ProcessStartInfo
                            {
                                FileName = "/bin/bash",
                                ArgumentList = { "-lc", $"node {EscapeBashArg(bridgePath)}" },
                                WorkingDirectory = repoRoot,
                                RedirectStandardOutput = true,
                                RedirectStandardError = true,
                                UseShellExecute = false,
                                CreateNoWindow = true
                            };
                            // Pass env flag to optionally disable thought logging
                            psi.Environment["DISABLE_THOUGHT_LOGGING"] = Environment.GetEnvironmentVariable("DISABLE_THOUGHT_LOGGING") ?? "false";
                            psi.Environment["MCP_SEQ_ARGS_B64"] = argsB64;

                            try
                            {
                                using var proc = System.Diagnostics.Process.Start(psi)!;
                                string stdout = await proc.StandardOutput.ReadToEndAsync();
                                string stderr = await proc.StandardError.ReadToEndAsync();
                                bool exited = proc.WaitForExit(20_000);
                                if (!exited)
                                {
                                    try { proc.Kill(entireProcessTree: true); } catch { }
                                    return JsonSerializer.Serialize(new { ok = false, error = "Timeout after 20s from Node bridge." }, new JsonSerializerOptions { WriteIndented = true });
                                }
                                if (!string.IsNullOrWhiteSpace(stderr))
                                {
                                    // Print stderr too so users can see any server logs
                                    Console.WriteLine(stderr);
                                }

                                if (string.IsNullOrWhiteSpace(stdout))
                                {
                                    // Return stderr in a JSON envelope if stdout is empty
                                    return JsonSerializer.Serialize(new { ok = false, error = string.IsNullOrWhiteSpace(stderr) ? "No output from Node bridge" : stderr.Trim() }, new JsonSerializerOptions { WriteIndented = true });
                                }

                                // The bridge prints JSON to stdout (return it to the tool message)
                                return stdout.Trim();
                            }
                            catch (Exception ex)
                            {
                                // Helpful guidance for missing Node / npx
                                return JsonSerializer.Serialize(new { ok = false, error = $"Failed to run Node bridge: {ex.Message}. Ensure Node.js is installed (e.g., brew install node)." }, new JsonSerializerOptions { WriteIndented = true });
                            }
                        }
                    default:
                        return $"Unknown tool: {toolCall.FunctionName}";
                }
            }
            catch (Exception ex)
            {
                return $"Error executing {toolCall.FunctionName}: {ex.Message}";
            }
        }

        private static string EscapeBashArg(string path)
        {
            if (string.IsNullOrEmpty(path)) return "''";
            // Simple safe single-quote escaping for bash
            return "'" + path.Replace("'", "'\\''") + "'";
        }

        // Dummy method names used for tool wiring; not called directly
        private static void ReadFileFunc() { }
        private static void WriteFileFunc() { }
        private static void SeeFileTreeFunc() { }
        private static void ExecuteBashCommandFunc() { }
        private static void SearchInFilesFunc() { }
        private static void SequentialThinkingFunc() { }
    }

    private static async Task InteractiveLoopAsync()
    {
        Console.WriteLine(new string('=', 80));
        Console.WriteLine("INTERACTIVE CODING AGENT - Have a conversation!");
        Console.WriteLine("Type your questions and press Enter. Use Ctrl+C to exit.");
        Console.WriteLine("I can help with file operations, code analysis, and general questions.");
        Console.WriteLine();
        Console.WriteLine("Tips:");
        Console.WriteLine("- Single-line: type and press Enter.");
        Console.WriteLine("- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.");
        Console.WriteLine("- Exit: type /exit or /quit.");
        Console.WriteLine(new string('=', 80));

        var client = CreateClient();
        var bot = new ChatBot(
            client,
            DeploymentName!,
            // Pass null to use the default system prompt with guardrails
            system: null,
            projectPath: Directory.GetParent(Environment.CurrentDirectory)?.FullName ?? Environment.CurrentDirectory
        );

        try
        {
            while (true)
            {
                Console.Write("\nYour question: ");
                var first = Console.ReadLine();
                if (first == null)
                {
                    Console.WriteLine("\nGoodbye! Thanks for chatting!");
                    break;
                }
                first = first.Trim();

                string userQuery;
                if (string.Equals(first, "/ml", StringComparison.OrdinalIgnoreCase))
                {
                    Console.WriteLine("Enter multi-line input. Finish with /end or --- on a line by itself.");
                    var lines = new List<string>();
                    while (true)
                    {
                        var line = Console.ReadLine();
                        if (line is null)
                        {
                            userQuery = string.Join('\n', lines).Trim();
                            break;
                        }
                        var trimmed = line.Trim();
                        if (trimmed.Equals("/end", StringComparison.OrdinalIgnoreCase) || trimmed == "---")
                        {
                            userQuery = string.Join('\n', lines).Trim();
                            break;
                        }
                        lines.Add(line);
                    }
                }
                else
                {
                    userQuery = first;
                }

                if (userQuery.Equals("/exit", StringComparison.OrdinalIgnoreCase) || userQuery.Equals("/quit", StringComparison.OrdinalIgnoreCase))
                {
                    Console.WriteLine("Exiting...");
                    break;
                }
                if (string.IsNullOrWhiteSpace(userQuery))
                {
                    Console.WriteLine("Please enter a question.");
                    continue;
                }

                try
                {
                    var answer = await bot.AskAsync(userQuery);
                    Console.WriteLine($"Answer: {answer}");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error calling model: {ex.Message}");
                }
            }
        }
        catch (OperationCanceledException)
        {
            Console.WriteLine("\nGoodbye! Thanks for chatting!");
        }
    }
}
