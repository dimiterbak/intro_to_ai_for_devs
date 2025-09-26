using OpenAI;
using OpenAI.Chat;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace DotNetOpenAI;

/// <summary>
/// C# port of python/react_example.py demonstrating the ReAct (Reason + Act) pattern.
/// Maintains a conversation where the model can request actions (wikipedia, calculate)
/// and receives observations in subsequent turns until it outputs an Answer.
/// </summary>
public static class ReActExample
{
    private static string? Endpoint => Environment.GetEnvironmentVariable("AI_ENDPOINT");
    private static string? DeploymentName => Environment.GetEnvironmentVariable("DEPLOYMENT_NAME");
    private static string? ApiKey => Environment.GetEnvironmentVariable("AI_API_KEY");

    private const string SystemPrompt = @"You run in a loop of Thought, Action, PAUSE, Observation.
At the end of the loop you output an Answer
Use Thought to describe your thoughts about the question you have been asked.
Use Action to run one of the actions available to you - then return PAUSE.
Observation will be the result of running those actions.

Your available actions are:

calculate:
e.g. calculate: 4 * 7 / 3
Runs a calculation and returns the number - uses C# style floating point syntax if necessary

wikipedia:
e.g. wikipedia: Django
Returns a summary from searching Wikipedia

Always look things up on Wikipedia if you have the opportunity to do so.

Example session:

Question: What is the capital of France?
Thought: I should look up France on Wikipedia
Action: wikipedia: France
PAUSE

You will be called again with this:

Observation: France is a country. The capital is Paris.

You then output:

Answer: The capital of France is Paris";

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
        Console.WriteLine("QUERY EXAMPLES - Demonstrating ReAct (Reason + Act) Pattern");
        Console.WriteLine(new string('=', 80));

        var examples = new[]
        {
            "What states does Ohio share borders with?",
            "Calculate the square root of 256.",
            "Who was the first president of the United States?",
            "How many r's are in strawberry?"
        };

        for (int i = 0; i < examples.Length; i++)
        {
            Console.WriteLine($"\n--- Example {i + 1} ---");
            Console.WriteLine(examples[i]);
        }
    }

    private static OpenAIClient CreateClient() => new(new System.ClientModel.ApiKeyCredential(ApiKey!), new OpenAIClientOptions
    {
        Endpoint = new Uri(Endpoint!)
    });

    private sealed class ReActSession
    {
        private readonly List<ChatMessage> _messages = new();
        private readonly OpenAIClient _client;
        private readonly string _deployment;
        private int _promptCount = 0;
        private static readonly Regex ActionRegex = new("^Action: (\\w+): (.*)$", RegexOptions.Multiline);

        public ReActSession(OpenAIClient client, string deployment)
        {
            _client = client;
            _deployment = deployment;
            _messages.Add(new SystemChatMessage(SystemPrompt));
        }

        public async Task QueryLoopAsync(string question, int maxTurns = 5)
        {
            Console.WriteLine($"\nQuestion: {question}");
            string nextPrompt = question;
            int i = 0;
            while (i < maxTurns)
            {
                string result = await SendAsync(nextPrompt);
                Console.WriteLine(result);
                var actions = ParseActions(result);
                if (actions.Count > 0)
                {
                    var (action, actionInput) = actions[0];
                    if (!KnownActions.ContainsKey(action))
                    {
                        Console.WriteLine($"Unknown action requested: {action}: {actionInput}");
                        return;
                    }
                    Console.WriteLine($" -- running {action} {actionInput}");
                    string observation;
                    try
                    {
                        observation = await KnownActions[action](actionInput);
                    }
                    catch (Exception ex)
                    {
                        observation = $"Error executing {action}: {ex.Message}";
                    }
                    Console.WriteLine($"Observation: {observation}");
                    nextPrompt = $"Observation: {observation}";
                    i++;
                }
                else
                {
                    return; // Model produced an Answer
                }
            }
        }

        private List<(string action, string input)> ParseActions(string content)
        {
            var results = new List<(string, string)>();
            foreach (Match m in ActionRegex.Matches(content))
            {
                if (m.Success)
                {
                    results.Add((m.Groups[1].Value.Trim(), m.Groups[2].Value.Trim()));
                }
            }
            return results;
        }

        private async Task<string> SendAsync(string userContent)
        {
            _messages.Add(new UserChatMessage(userContent));
            _promptCount++;

            Console.WriteLine($"\n-- Prompt {_promptCount} --");
            Console.WriteLine("\n=== FULL PROMPT SENT TO MODEL ===");
            foreach (var msg in _messages)
            {
                var content = string.Join("\n", msg.Content.Select(c => c.Text));
                Console.WriteLine($"  {content}\n");
            }
            var chatClient = _client.GetChatClient(_deployment);
            var response = await chatClient.CompleteChatAsync(_messages);
            var assistantText = response.Value.Content[0].Text;
            _messages.Add(new AssistantChatMessage(assistantText));
            Console.WriteLine("=== END OF PROMPT ===\n");
            return assistantText;
        }
    }

    // Action implementations
    private static async Task<string> WikipediaAsync(string query)
    {
        try
        {
            using var http = new HttpClient();
            // Wikipedia requires a descriptive User-Agent per their API etiquette
            http.DefaultRequestHeaders.UserAgent.ParseAdd("IntroToAIReActBot/1.0 (https://example.com; contact=you@example.com)");
            http.DefaultRequestHeaders.Accept.ParseAdd("application/json");

            var url = "https://en.wikipedia.org/w/api.php";
            var parameters = new Dictionary<string, string>
            {
                ["action"] = "query",
                ["list"] = "search",
                ["srsearch"] = query,
                ["format"] = "json",
                ["utf8"] = "1"
            };
            var requestUri = $"{url}?{string.Join("&", parameters.Select(kv => $"{Uri.EscapeDataString(kv.Key)}={Uri.EscapeDataString(kv.Value)}"))}";

            using var resp = await http.GetAsync(requestUri);
            if (!resp.IsSuccessStatusCode)
            {
                return $"Wikipedia request failed: {(int)resp.StatusCode} {resp.ReasonPhrase}";
            }
            var json = await resp.Content.ReadAsStringAsync();
            using var doc = JsonDocument.Parse(json);
            if (!doc.RootElement.TryGetProperty("query", out var queryEl) ||
                !queryEl.TryGetProperty("search", out var searchArr) ||
                searchArr.GetArrayLength() == 0)
            {
                return "No results";
            }
            var snippet = searchArr[0].GetProperty("snippet").GetString() ?? "No result";
            // Strip basic HTML tags that Wikipedia returns in snippet
            snippet = Regex.Replace(snippet, "<.*?>", string.Empty);
            snippet = System.Net.WebUtility.HtmlDecode(snippet);
            return snippet.Trim();
        }
        catch (Exception ex)
        {
            return $"Wikipedia error: {ex.Message}";
        }
    }

    private static Task<string> CalculateAsync(string expression)
    {
        // Very simple and unsafe expression evaluator placeholder.
        // For safety, only allow digits, operators, parentheses, decimal points, and whitespace.
    if (!Regex.IsMatch(expression, "^[0-9+\\-*/(). %]+$"))
        {
            return Task.FromResult("Unsupported characters in calculation.");
        }
        try
        {
            var result = new System.Data.DataTable().Compute(expression, null);
            return Task.FromResult(Convert.ToString(result) ?? "(null)");
        }
        catch (Exception ex)
        {
            return Task.FromResult($"Error: {ex.Message}");
        }
    }

    private static readonly Dictionary<string, Func<string, Task<string>>> KnownActions = new(StringComparer.OrdinalIgnoreCase)
    {
        ["wikipedia"] = WikipediaAsync,
        ["calculate"] = CalculateAsync
    };

    private static async Task InteractiveLoopAsync()
    {
        Console.WriteLine(new string('=', 80));
        Console.WriteLine("INTERACTIVE MODE - Ask your own questions!");
        Console.WriteLine("Type your questions and press Enter. Ctrl+C to exit.");
        Console.WriteLine(new string('=', 80));

        var client = CreateClient();
        var session = new ReActSession(client, DeploymentName!);
        try
        {
            while (true)
            {
                Console.Write("\nYour question: ");
                var input = Console.ReadLine();
                if (input == null)
                {
                    Console.WriteLine("\nGoodbye! Thanks for using the ReAct agent.");
                    break;
                }
                input = input.Trim();
                if (input.Length == 0)
                {
                    Console.WriteLine("Please enter a question.");
                    continue;
                }
                await session.QueryLoopAsync(input);
            }
        }
        catch (OperationCanceledException)
        {
            Console.WriteLine("\nGoodbye! Thanks for using the ReAct agent.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Unexpected error: {ex.Message}");
        }
    }
}
