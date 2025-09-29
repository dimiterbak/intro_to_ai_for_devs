using OpenAI;
using OpenAI.Chat;

namespace DotNetOpenAI;

public class TestOpenAI
{
    public static async Task RunTest()
    {
        // Make sure to set your environment variables accordingly
        string? endpoint = Environment.GetEnvironmentVariable("AI_ENDPOINT");
        string? deploymentName = Environment.GetEnvironmentVariable("DEPLOYMENT_NAME");
        string? apiKey = Environment.GetEnvironmentVariable("AI_API_KEY");

        // Validate required environment variables
        if (string.IsNullOrEmpty(apiKey))
        {
            Console.Error.WriteLine("Error: AI_API_KEY environment variable is not set");
            Environment.Exit(1);
        }

        if (string.IsNullOrEmpty(endpoint))
        {
            Console.Error.WriteLine("Error: AI_ENDPOINT environment variable is not set");
            Environment.Exit(1);
        }

        if (string.IsNullOrEmpty(deploymentName))
        {
            Console.Error.WriteLine("Error: DEPLOYMENT_NAME environment variable is not set");
            Environment.Exit(1);
        }

        var client = new OpenAIClient(new System.ClientModel.ApiKeyCredential(apiKey), new OpenAIClientOptions
        {
            Endpoint = new Uri(endpoint)
        });

        var response = await client.GetChatClient(deploymentName).CompleteChatAsync(
            [
                new UserChatMessage("Hello, what is the capital of France?")
            ]
        );

        Console.WriteLine(response.Value.Content[0].Text);
    }
}