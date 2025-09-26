using DotNetOpenAI;

// Check command line arguments to determine which app to run
if (args.Length > 0)
{
    switch (args[0].ToLower())
    {
        case "test":
        case "testopenai":
            await TestOpenAI.RunTest();
            break;
        case "chatbot":
            await ChatBotExample.RunAsync();
            break;
        case "react":
        case "reactexample":
            await ReActExample.RunAsync();
            break;
        case "help":
        case "--help":
        case "-h":
            ShowHelp();
            break;
        default:
            Console.WriteLine($"Unknown app: {args[0]}");
            ShowHelp();
            break;
    }
}
else
{
    // Default app when no arguments provided
    await TestOpenAI.RunTest();
}

static void ShowHelp()
{
    Console.WriteLine("Usage: dotnet run <app-name>");
    Console.WriteLine();
    Console.WriteLine("Available apps:");
    Console.WriteLine("  test         - Run OpenAI test");
    Console.WriteLine("  testopenai   - Run OpenAI test (alias)");
    Console.WriteLine("  chatbot      - Run interactive chatbot example");
    Console.WriteLine("  react        - Run ReAct (Reason + Act) example");
    Console.WriteLine("  reactexample - Run ReAct (alias)");
    Console.WriteLine("  help         - Show this help message");
    Console.WriteLine();
    Console.WriteLine("Examples:");
    Console.WriteLine("  dotnet run                  # Run default app (test)");
    Console.WriteLine("  dotnet run test             # Run OpenAI test");
    Console.WriteLine("  dotnet run chatbot          # Run chatbot example");
    Console.WriteLine("  dotnet run react            # Run ReAct example");
    Console.WriteLine("  dotnet run help             # Show this help");
}