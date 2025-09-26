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

        if (string.IsNullOrEmpty(endpoint) || string.IsNullOrEmpty(deploymentName) || string.IsNullOrEmpty(apiKey))
        {
            Console.WriteLine("Error: Please set the AI_ENDPOINT, DEPLOYMENT_NAME, and AI_API_KEY environment variables.");
            return;
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