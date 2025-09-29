# Node Version of Workshop

## Prerequisites

Make sure you have Node.js installed on your system. You can download it from [nodejs.org](https://nodejs.org/).

Check if Node.js is installed:
```bash
node --version
npm --version
```

## 1. Navigate to the Node.js project directory
```bash
cd /Users/dimitarbakardzhiev/git/intro_to_ai_for_devs/node
```

## 2. Install dependencies
```bash
npm install
```

This will install:
- `openai` - The OpenAI SDK for Node.js
- `axios` - HTTP client used by examples
- `readline-sync` - Simple input utility used by examples
- `typescript` - TypeScript compiler
- `ts-node` - TypeScript execution environment
- `@types/node` - Node.js type definitions
- `@modelcontextprotocol/sdk` - MCP JS client SDK used to call the Sequential Thinking server tool

Optional but recommended for offline/first-run speed:
```bash
# Prefetch the Sequential Thinking MCP server (otherwise npx will fetch it on first use)
npm install -D @modelcontextprotocol/server-sequential-thinking
```

## 3. Export your environment variables

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

## 4. Run a simple test

The `test_openai.ts` file is already created for you:

### Run it with test_openai.js:
```bash
npm run dev
```

If everything is set up correctly, you'll see a reply "The capital of France is Paris." from the model.

## Available Scripts

- `npm run dev` - Run TypeScript files directly with ts-node
- `npm run build` - Compile TypeScript to JavaScript
- `npm start` - Run the compiled JavaScript
- `npm test` - Build and run the project
- `npm run react`- Run ReAct (Reason + Act) example
- `npm run coding-agent:mcp` - Run the coding agent that exposes the `sequentialthinking` MCP tool

## Troubleshooting

1. **Module not found errors**: Make sure you've run `npm install`
2. **Environment variable issues**: Double-check that your environment variables are set correctly
3. **TypeScript compilation errors**: Check that your `tsconfig.json` is properly configured. If you haven't installed the MCP SDK yet, we've added lightweight type shims so the project still compiles; install the real SDK with `npm install @modelcontextprotocol/sdk` when you want to use the MCP tool.
4. **API errors**: Verify your API key, endpoint, and deployment name are correct

### Using the MCP "Sequential Thinking" tool

The TypeScript coding agent (`coding_agent_mcp_example.ts`) can call the MCP server tool named `sequentialthinking`.

- Ensure Node.js and `npx` are available in your PATH
- Either rely on `npx -y @modelcontextprotocol/server-sequential-thinking` (download on first run), or pre-install as shown above
- Optional privacy toggle: set `DISABLE_THOUGHT_LOGGING=true` to suppress thought logs from the server

Run the demo:
```bash
npm run coding-agent:mcp
```
