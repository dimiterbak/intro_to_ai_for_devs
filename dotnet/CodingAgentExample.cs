using OpenAI;
using OpenAI.Chat;
using System.Text.Json;

namespace DotNetOpenAI;

/// <summary>
/// A C# port of python/coding_agent_example.py using tools (function calling).
/// It exposes filesystem and shell tools and lets the model call them iteratively
/// until a final answer is produced.
/// </summary>
public static class CodingAgentExample
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
        private int _promptCount = 0;

        // Define Chat tools (function calling) compatible with OpenAI .NET SDK
        private readonly ChatTool _readFileTool;
        private readonly ChatTool _writeFileTool;
        private readonly ChatTool _seeFileTreeTool;
        private readonly ChatTool _executeBashTool;
        private readonly ChatTool _searchInFilesTool;

        public ChatBot(OpenAIClient client, string deployment, string? system, string projectPath)
        {
            _client = client;
            _deployment = deployment;
            _tools = new CodingAgentTools(projectPath);
            if (!string.IsNullOrWhiteSpace(system))
            {
                _messages.Add(new SystemChatMessage(system!));
            }

            _readFileTool = ChatTool.CreateFunctionTool(
                functionName: nameof(ReadFileFunc),
                functionDescription: "Read and return the contents of a file at the given relative filepath",
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
                functionDescription: "Execute a bash command in the shell and return its output, error, and exit code",
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
        }

        public async Task<string> AskAsync(string userContent)
        {
            _messages.Add(new UserChatMessage(userContent));
            return await ExecuteAsync();
        }

        private ChatCompletionOptions BuildOptions() => new()
        {
            Tools = { _readFileTool, _writeFileTool, _seeFileTreeTool, _executeBashTool, _searchInFilesTool }
        };

        private async Task<string> ExecuteAsync()
        {
            _promptCount++;
            Console.WriteLine($"\n-- Prompt {_promptCount} --");
            Console.WriteLine("\n=== FULL PROMPT SENT TO MODEL ===");
            foreach (var msg in _messages)
            {
                var content = string.Join("\n", msg.Content.Select(c => c.Text));
                Console.WriteLine($"  {content}\n");
            }

            var chatClient = _client.GetChatClient(_deployment);
            var options = BuildOptions();
            var completion = await chatClient.CompleteChatAsync(_messages, options);
            Console.WriteLine("=== END OF PROMPT ===\n");

            // Handle tool calls loop if needed
            var result = completion.Value;
            if (result.FinishReason == ChatFinishReason.ToolCalls)
            {
                // Add assistant message with tool calls to history
                _messages.Add(new AssistantChatMessage(result));

                foreach (var toolCall in result.ToolCalls)
                {
                    Console.WriteLine($"🔧 Executing tool: {toolCall.FunctionName} with args: {toolCall.FunctionArguments}");
                    var toolResult = await ExecuteToolAsync(toolCall);
                    _messages.Add(new ToolChatMessage(toolCall.Id, toolResult));
                }

                // Ask model again with tool results
                return await ExecuteAsync();
            }
            else
            {
                var text = result.Content.Count > 0 ? result.Content[0].Text : string.Empty;
                _messages.Add(new AssistantChatMessage(result));
                return text;
            }
        }

        private async Task<string> ExecuteToolAsync(ChatToolCall toolCall)
        {
            try
            {
                // toolCall.FunctionArguments is a stringified JSON per docs
                using var argsJson = JsonDocument.Parse(toolCall.FunctionArguments);
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
                    default:
                        return $"Unknown tool: {toolCall.FunctionName}";
                }
            }
            catch (Exception ex)
            {
                return $"Error executing {toolCall.FunctionName}: {ex.Message}";
            }
        }

        // Dummy method names used for tool wiring; not called directly
        private static void ReadFileFunc() { }
        private static void WriteFileFunc() { }
        private static void SeeFileTreeFunc() { }
        private static void ExecuteBashCommandFunc() { }
        private static void SearchInFilesFunc() { }
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
            system: "You are a helpful coding assistant with access to file system tools. You can read files, write files, see directory structures, execute bash commands, and search in files. Use these tools to help with coding tasks and file management.",
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
