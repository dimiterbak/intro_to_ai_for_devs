# Introduction to AI for .Net Devs

## Prerequisites

Make sure you have .NET 9.0 SDK installed. You can download it from [https://dotnet.microsoft.com/download](https://dotnet.microsoft.com/download).

Verify your installation:
```
dotnet --version
```

## 1. Navigate to the dotnet project directory
```
cd dotnet
```

## 2. Install the OpenAI SDK
The OpenAI package should already be installed, but if you need to add it:
```
dotnet add package OpenAI
```

## 3. Restore dependencies
```
dotnet restore
```

## 4. Export your environment variables

### Windows PowerShell:
```
$env:AI_API_KEY = "your_api_key_here"
$env:AI_ENDPOINT = "your_endpoint_key_here"
$env:DEPLOYMENT_NAME = "your_model_name-here"
```

Or for persistent environment variables (requires restart of terminal):
```
setx AI_API_KEY "your_api_key_here"
setx AI_ENDPOINT "your_endpoint_key_here"
setx DEPLOYMENT_NAME "your_model_name-here"
```

### Linux / macOS:
```
export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpoint_key_here"
export DEPLOYMENT_NAME="your_model_name-here"
```

## 5. Build the project
```
dotnet build
```

## 6. Run applications

The project uses a command line argument system to run different console applications. You can run specific apps or get help about available options.

### Available Commands:

```bash
# Run the default app (TestOpenAI)
dotnet run

# Run TestOpenAI explicitly  
dotnet run test
# or
dotnet run testopenai

# Show help and list all available apps
dotnet run help
# or
dotnet run --help
# or
dotnet run -h
```

### Example Usage:

```bash
# Default behavior - runs TestOpenAI
PS> dotnet run
The capital of France is Paris.

# Show available apps
PS> dotnet run help
Usage: dotnet run <app-name>

Available apps:
  test         - Run OpenAI test
  testopenai   - Run OpenAI test (alias)
  chatbot      - Run interactive chatbot example
  react        - Run ReAct (Reason + Act) example
  reactexample - Run ReAct (alias)
  reactcount   - Run ReAct with count_letters tool
  reactcountexample - Run ReAct with count_letters (alias)
  codingagent  - Run tool-enabled coding agent example
  help         - Show this help message


Examples:
  dotnet run           # Run default app (test)
  dotnet run test      # Run OpenAI test
  dotnet run help      # Show this help

# Run specific app
PS> dotnet run test
The capital of France is Paris.
```

### Adding New Applications:

When you create new console applications in this project:

1. **Create your new class** with a static method (e.g., `MyNewApp.cs`):
   ```csharp
   namespace DotNetOpenAI;
   
   public class MyNewApp
   {
       public static async Task RunApp()
       {
           Console.WriteLine("Running my new app!");
           // Your app logic here
       }
   }
   ```

2. **Add a case to Program.cs** in the switch statement:
   ```csharp
   case "mynewapp":
   case "newapp":
       await MyNewApp.RunApp();
       break;
   ```

3. **Update the help text** in the `ShowHelp()` method to include your new app.

If everything is set up correctly, you'll see a reply from the model when running the test app.

## 7. Development workflow

### Build the project:
```
dotnet build
```

### Run applications:
```
# Run default app
dotnet run

# Run specific app
dotnet run <app-name>

# Show available apps
dotnet run help
```

### Add new packages:
```
dotnet add package <PackageName>
```

### Clean build artifacts:
```
dotnet clean
```

## Troubleshooting

- **Missing environment variables**: Make sure all three environment variables (`AI_API_KEY`, `AI_ENDPOINT`, `DEPLOYMENT_NAME`) are set correctly
- **Build errors**: Run `dotnet clean` then `dotnet build` to rebuild from scratch
- **Package issues**: Run `dotnet restore` to restore NuGet packages
- **Runtime errors**: Check that your API endpoint URL is correct and includes the proper protocol (https://)

### VS Code shows MSBuild/hostfxr errors on macOS

If you see messages like the following in VS Code when opening `.cs` files:

```
Failed to obtain virtual project ... using dotnet run-api
Failed to find all versions of .NET Core MSBuild. Call to hostfxr_resolve_sdk2
```

This almost always means the .NET SDK isn’t installed or VS Code isn’t seeing it on your PATH.

Fix steps (macOS):

1) Check if `dotnet` is available in a terminal:

```bash
dotnet --info
```

If this prints “command not found”, install the SDK.

2) Install the .NET SDK 9.0 (recommended for this repo’s `TargetFramework=net9.0`):

- Option A: Official installer (works on Intel and Apple Silicon)
  - Go to https://dotnet.microsoft.com/download/dotnet/9.0
  - Download the macOS SDK (.pkg) matching your chip (Apple Silicon arm64 or Intel x64)
  - Run the installer

- Option B: Homebrew (note: may lag behind latest SDK)

```bash
brew update
brew install --cask dotnet-sdk
```

If you specifically need .NET 9 and Homebrew doesn’t provide it, prefer the official installer above.

3) Ensure PATH picks up `dotnet`:

```bash
# Common install locations (adjust if needed)
export PATH="$PATH:/usr/local/share/dotnet:/opt/homebrew/share/dotnet"

# Optional: create a symlink if /usr/local/bin isn’t present
sudo ln -sfn \
  /usr/local/share/dotnet/dotnet \
  /usr/local/bin/dotnet 2>/dev/null || true
```

Add the PATH line to your shell profile to make it persistent (e.g. `~/.bash_profile`, `~/.bashrc`, or `~/.zshrc`).

4) Restart VS Code so it inherits the updated PATH.
   - Best practice: launch VS Code from a terminal that already has `dotnet` on PATH (e.g. `code .`).

5) Verify inside VS Code terminal:

```bash
which dotnet
dotnet --list-sdks
```

You should see a 9.0.x SDK listed. If so, the C# language server should resolve MSBuild correctly.

6) If you can’t install .NET 9 right now, you can temporarily switch the project to .NET 8:

- Edit `dotnet.csproj` and change:

```xml
<TargetFramework>net9.0</TargetFramework>
```

to:

```xml
<TargetFramework>net8.0</TargetFramework>
```

Then run:

```bash
cd dotnet
dotnet restore
dotnet build
```

Switch back to `net9.0` when you have the .NET 9 SDK installed.