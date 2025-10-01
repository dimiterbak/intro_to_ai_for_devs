using OpenAI;
using OpenAI.Chat;

namespace DotNetOpenAI;

/// <summary>
/// ChatBot example mirroring the python/chatbot_example.py behavior.
/// Maintains conversation history and allows interactive querying.
/// Environment variables required:
///   AI_ENDPOINT       - base endpoint (e.g. https://my-azure-openai.openai.azure.com/)
///   DEPLOYMENT_NAME   - model deployment / model name
///   AI_API_KEY        - API key
/// </summary>
public static class ChatBotExample
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
        Console.WriteLine("CHATBOT EXAMPLES - Simple Q&A");
        Console.WriteLine(new string('=', 80));

        var examples = new[]
        {
            "What states does Ohio share borders with?",
            "Calculate the square root of 256.",
            "How many r's are in strawbery?",
            "Who was the first president of the United States?"
        };

        for (int i = 0; i < examples.Length; i++)
        {
            Console.WriteLine($"\n--- Example {i + 1} ---");
            Console.WriteLine(examples[i]);
        }
        Console.WriteLine();
    }

    private static OpenAIClient CreateClient() => new(new System.ClientModel.ApiKeyCredential(ApiKey!), new OpenAIClientOptions
    {
        Endpoint = new Uri(Endpoint!)
    });

    /// <summary>
    /// Represents a simple chat session maintaining message history.
    /// </summary>
    private sealed class ChatSession
    {
        private readonly List<ChatMessage> _messages = new();
        private readonly OpenAIClient _client;
        private readonly string _deployment;
        private int _promptCount = 0;

        public ChatSession(OpenAIClient client, string deployment, string? system = null)
        {
            _client = client;
            _deployment = deployment;
            if (!string.IsNullOrWhiteSpace(system))
            {
                _messages.Add(new SystemChatMessage(system!));
            }
        }

        public async Task<string> SendAsync(string userContent)
        {
            _messages.Add(new UserChatMessage(userContent));
            _promptCount++;

            Console.WriteLine($"\n-- Prompt {_promptCount} --");
            Console.WriteLine("\n=== FULL PROMPT SENT TO MODEL ===");
            foreach (var msg in _messages)
            {
                var role = msg switch
                {
                    SystemChatMessage => "system",
                    UserChatMessage => "user",
                    AssistantChatMessage => "assistant",
                    _ => "other"
                };

                var content = string.Join("\n", msg.Content.Select(c => c.Text));
                Console.WriteLine($"  {content}\n");
            }

            var chatClient = _client.GetChatClient(_deployment);
            var response = await chatClient.CompleteChatAsync(_messages);

            var assistantText = response.Value.Content[0].Text;
            _messages.Add(new AssistantChatMessage(assistantText));

            // Print out token usage each time (pretty-printed; SDK shapes may vary)
            var usage = response.Value.Usage;
            if (usage is not null)
            {
                // Console.WriteLine("Token usage:");
                var props = usage.GetType().GetProperties();
                foreach (var p in props)
                {
                    object? val = null;
                    try { val = p.GetValue(usage); }
                    catch { /* ignore */ }
                    Console.WriteLine($"  {p.Name}: {val}");
                }
            }
        
            Console.WriteLine("=== END OF PROMPT ===\n");
            return assistantText;
        }
    }

    private static async Task InteractiveLoopAsync()
    {
        Console.WriteLine(new string('=', 80));
        Console.WriteLine("INTERACTIVE CHATBOT - Have a conversation!");
        Console.WriteLine("Type your questions and press Enter. Ctrl+C to exit.");
        Console.WriteLine("Tips:");
        Console.WriteLine("- Single-line: type and press Enter.");
        Console.WriteLine("- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.");
        Console.WriteLine("- Exit: type /exit or /quit.");
        Console.WriteLine(new string('=', 80));

        var client = CreateClient();
        var session = new ChatSession(client, DeploymentName!, "You are a helpful assistant.");

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
                            userQuery = string.Join("\n", lines).Trim();
                            break;
                        }
                        var trimmed = line.Trim();
                        if (trimmed.Equals("/end", StringComparison.OrdinalIgnoreCase) || trimmed == "---")
                        {
                            userQuery = string.Join("\n", lines).Trim();
                            break;
                        }
                        else
                        {
                            lines.Add(line);
                        }
                    }
                }
                else
                {
                    userQuery = first;
                }

                // Allow quick exit commands
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

                string answer;
                try
                {
                    answer = await session.SendAsync(userQuery);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error calling model: {ex.Message}");
                    continue;
                }

                Console.WriteLine($"Answer: {answer}");
            }
        }
        catch (OperationCanceledException)
        {
            Console.WriteLine("\nGoodbye! Thanks for chatting!");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Unexpected error: {ex.Message}");
        }
    }
}
