import OpenAI from 'openai';
import * as readline from 'readline';
import * as path from 'path';
import { CodingAgentTools, CommandResult, SearchMatch } from './coding_agent_tools';

// Make sure to set your environment variables accordingly
const ENDPOINT = process.env.AI_ENDPOINT;
const DEPLOYMENT_NAME = process.env.DEPLOYMENT_NAME;
const API_KEY = process.env.AI_API_KEY;

interface Message {
    role: 'system' | 'user' | 'assistant' | 'tool';
    content: string;
    tool_calls?: OpenAI.Chat.Completions.ChatCompletionMessageToolCall[];
    tool_call_id?: string;
}

interface ToolDefinition {
    type: 'function';
    function: {
        name: string;
        description: string;
        parameters: {
            type: 'object';
            properties: Record<string, any>;
            required: string[];
        };
    };
}

class ChatBot {
    private numberOfPromptsSent: number = 0;
    private agentTools: CodingAgentTools;
    private system: string;
    public messages: Message[] = [];
    private client: OpenAI;

    constructor(system: string = "", projectPath?: string) {
        this.numberOfPromptsSent = 0;
        const resolvedProjectPath = projectPath || process.cwd();
        this.agentTools = new CodingAgentTools(resolvedProjectPath);
        this.system = system;
        this.messages = [];
        
        if (this.system) {
            this.messages.push({ role: "system", content: system });
        }

        this.client = new OpenAI({
            baseURL: ENDPOINT,
            apiKey: API_KEY
        });
    }

    /**
     * Convert agent tools to OpenAI function calling format
     */
    getToolDefinitions(): ToolDefinition[] {
        return [
            {
                type: "function",
                function: {
                    name: "read_file",
                    description: "Read and return the contents of a file at the given relative filepath",
                    parameters: {
                        type: "object",
                        properties: {
                            filepath: {
                                type: "string",
                                description: "Path to the file, relative to the project directory"
                            }
                        },
                        required: ["filepath"]
                    }
                }
            },
            {
                type: "function",
                function: {
                    name: "write_file",
                    description: "Write content to a file at the given relative filepath, creating directories as needed",
                    parameters: {
                        type: "object",
                        properties: {
                            filepath: {
                                type: "string",
                                description: "Path to the file, relative to the project directory"
                            },
                            content: {
                                type: "string",
                                description: "Content to write to the file"
                            }
                        },
                        required: ["filepath", "content"]
                    }
                }
            },
            {
                type: "function",
                function: {
                    name: "see_file_tree",
                    description: "Return a list of all files and directories under the given root directory",
                    parameters: {
                        type: "object",
                        properties: {
                            root_dir: {
                                type: "string",
                                description: "Root directory to list from, relative to the project directory",
                                default: "."
                            }
                        },
                        required: []
                    }
                }
            },
            {
                type: "function",
                function: {
                    name: "execute_bash_command",
                    description: "Execute a bash command in the shell and return its output, error, and exit code",
                    parameters: {
                        type: "object",
                        properties: {
                            command: {
                                type: "string",
                                description: "The bash command to execute"
                            },
                            cwd: {
                                type: "string",
                                description: "Working directory to run the command in, relative to the project directory"
                            }
                        },
                        required: ["command"]
                    }
                }
            },
            {
                type: "function",
                function: {
                    name: "search_in_files",
                    description: "Search for a pattern in all files under the given root directory",
                    parameters: {
                        type: "object",
                        properties: {
                            pattern: {
                                type: "string",
                                description: "Pattern to search for in files"
                            },
                            root_dir: {
                                type: "string",
                                description: "Root directory to search from, relative to the project directory",
                                default: "."
                            }
                        },
                        required: ["pattern"]
                    }
                }
            },
            {
                type: "function",
                function: {
                    name: "sequentialthinking",
                    description: "Facilitates a detailed, step-by-step thinking process for problem-solving and analysis using an MCP server.",
                    parameters: {
                        type: "object",
                        properties: {
                            thought: { type: "string", description: "Your current thinking step." },
                            nextThoughtNeeded: { type: "boolean", description: "Whether another thought step is needed." },
                            thoughtNumber: { type: "integer", description: "Current thought number (1-based)." },
                            totalThoughts: { type: "integer", description: "Estimated total thoughts needed." },
                            isRevision: { type: "boolean", description: "Whether this revises previous thinking." },
                            revisesThought: { type: "integer", description: "Which thought is being reconsidered." },
                            branchFromThought: { type: "integer", description: "Branching point thought number." },
                            branchId: { type: "string", description: "Branch identifier." },
                            needsMoreThoughts: { type: "boolean", description: "If reaching end but realizing more thoughts are needed." }
                        },
                        required: ["thought", "nextThoughtNeeded", "thoughtNumber", "totalThoughts"]
                    }
                }
            }
        ];
    }

    /**
     * Execute a tool function with the given arguments
     */
    async executeTool(toolName: string, args: Record<string, any>): Promise<string | CommandResult | string[] | SearchMatch[] | any> {
        try {
            switch (toolName) {
                case "read_file":
                    return this.agentTools.readFile(args.filepath);

                case "write_file":
                    this.agentTools.writeFile(args.filepath, args.content);
                    return `Successfully wrote to ${args.filepath}`;

                case "see_file_tree": {
                    const rootDir = args.root_dir || ".";
                    return this.agentTools.seeFileTree(rootDir);
                }

                case "execute_bash_command": {
                    const command = args.command;
                    const cwd = args.cwd;
                    return this.agentTools.executeBashCommand(command, cwd);
                }

                case "search_in_files": {
                    const pattern = args.pattern;
                    const searchRootDir = args.root_dir || ".";
                    return this.agentTools.searchInFiles(pattern, searchRootDir);
                }

                case "sequentialthinking":
                    return await this.callSequentialThinking(args);

                default:
                    return `Unknown tool: ${toolName}`;
            }
        } catch (error) {
            return `Error executing ${toolName}: ${error instanceof Error ? error.message : String(error)}`;
        }
    }

    /**
     * Bridge call to the sequentialthinking MCP server via stdio in Node.
     * This launches the MCP server using npx and calls its 'sequentialthinking' tool.
     * Returns either structured content (object) or a text summary string.
     */
    private async callSequentialThinking(args: Record<string, any>): Promise<any> {
        // Lazy-load the MCP SDK so the rest of the app runs without it
        let ClientCtor: any;
        let StdioClientTransportCtor: any;
        try {
            // Newer SDK path exports
            const clientMod = await import('@modelcontextprotocol/sdk/client/index.js');
            const stdioMod = await import('@modelcontextprotocol/sdk/client/stdio.js');
            ClientCtor = (clientMod as any).Client;
            StdioClientTransportCtor = (stdioMod as any).StdioClientTransport;
        } catch (e) {
            try {
                // Fallback to top-level export (older/alternative builds)
                // @ts-ignore - optional dependency; types may not be present
                const sdk = await import('@modelcontextprotocol/sdk');
                ClientCtor = (sdk as any).Client ?? (sdk as any).default?.Client;
                StdioClientTransportCtor = (sdk as any).StdioClientTransport ?? (sdk as any).default?.StdioClientTransport;
            } catch (err) {
                return (
                    'MCP client not installed. Please install the JS SDK with: npm install @modelcontextprotocol/sdk and ensure Node.js and npx are available.'
                );
            }
        }

        // If still missing after import attempts, return a helpful message
        if (!ClientCtor || !StdioClientTransportCtor) {
            return (
                'Failed to load MCP JS SDK. Ensure @modelcontextprotocol/sdk is installed and accessible.'
            );
        }

        // Create stdio transport that spawns the server via npx
        const transport = new StdioClientTransportCtor({
            command: 'npx',
            args: ['-y', '@modelcontextprotocol/server-sequential-thinking'],
            env: {
                ...process.env,
                DISABLE_THOUGHT_LOGGING: process.env.DISABLE_THOUGHT_LOGGING ?? 'false',
            },
        });

        const client = new ClientCtor({
            name: 'sequentialthinking-ts-client',
            version: '1.0.0',
        });

        try {
            await client.connect(transport);

            // Attempt to find the tool name as registered by the server (normalize separators)
            const list = await client.listTools();
            const desired = 'sequentialthinking';
            const found = list.tools.find((t: any) =>
                String(t.name).replace(/[-_]/g, '').toLowerCase() === desired
            );
            const toolName = found?.name ?? 'sequentialthinking';

            const result = await client.callTool({ name: toolName, arguments: args });

            // Close cleanly if supported
            if (typeof client.close === 'function') {
                await client.close();
            }
            if (typeof transport.close === 'function') {
                await transport.close();
            }

            return result;
        } catch (error: any) {
            if (error?.code === 'ENOENT') {
                return (
                    'Failed to launch MCP server via npx. Ensure Node.js and npx are installed and available in PATH. Try: brew install node'
                );
            }
            return `Error calling sequentialthinking via MCP: ${error?.message ?? String(error)}`;
        }
    }

    async call(message: string): Promise<OpenAI.Chat.Completions.ChatCompletionMessage> {
        this.messages.push({ role: "user", content: message });
        const responseMessage = await this.execute();
        
        // Return the response message object for query() to handle tool calling
        return responseMessage;
    }

    async execute(): Promise<OpenAI.Chat.Completions.ChatCompletionMessage> {
        this.numberOfPromptsSent++;
        console.log(`\n-- Prompt ${this.numberOfPromptsSent} --`);
        console.log("\n=== FULL PROMPT SENT TO MODEL ===");
        
        // Show the complete prompt being sent to the model
        this.messages.forEach((msg) => {
            console.log(`  ${msg.content}`);
            console.log();
        });

        // Include tools in the API call
        const response = await this.client.chat.completions.create({
            model: DEPLOYMENT_NAME!,
            messages: this.messages as OpenAI.Chat.Completions.ChatCompletionMessageParam[],
            tools: this.getToolDefinitions() as OpenAI.Chat.Completions.ChatCompletionTool[]
        });

        console.log("=== END OF PROMPT ===\n");
        return response.choices[0].message;
    }
}

function showExamples(): void {
    console.log("=".repeat(80));
    console.log("CHATBOT EXAMPLES - Tool-Enabled Coding Agent");
    console.log("=".repeat(80));
    
    const examples = [
        "Show me the file structure of this project",
        "Read the README.md file",
        "Search for any TypeScript files that contain 'OpenAI'",
        "What states does Ohio share borders with?",  // Regular question without tools
        "Create a simple hello world TypeScript script in a new file called hello.ts"
    ];
    
    for (let i = 0; i < examples.length; i++) {
        console.log(`\n--- Example ${i + 1} ---`);
        console.log(`${examples[i]}`);
    }
}

async function query(question: string, bot: ChatBot): Promise<string> {
    let responseMessage = await bot.call(question);

    // Keep handling tool calls until the model returns a final answer
    const maxToolCycles = 8;
    let cycles = 0;
    const toolResultsAccum: string[] = [];

    while (true) {
        if (responseMessage.tool_calls && responseMessage.tool_calls.length > 0) {
            cycles += 1;
            if (cycles > maxToolCycles) {
                // Safety fallback: break out and return a summary of the last tool output if any
                const last = toolResultsAccum[toolResultsAccum.length - 1];
                const summary = last
                    ? "Stopped after max tool cycles. Latest tool result:\n" + last
                    : (responseMessage.content || "Stopped after max tool cycles with no final content.");
                bot.messages.push({ role: "assistant", content: summary });
                return summary;
            }

            // Add the assistant's response to conversation history
            bot.messages.push({
                role: "assistant",
                content: responseMessage.content || "",
                tool_calls: responseMessage.tool_calls,
            });

            // Execute each tool call
            for (const toolCall of responseMessage.tool_calls) {
                const functionName = toolCall.function.name;
                const functionArgs = JSON.parse(toolCall.function.arguments);

                console.log(`🔧 Executing tool: ${functionName} with args:`, functionArgs);

                // Execute the tool
                const toolResult = await bot.executeTool(functionName, functionArgs);

                // Keep a readable transcript of tool outputs
                const display = typeof toolResult === 'string' ? toolResult : JSON.stringify(toolResult, null, 2);
                toolResultsAccum.push(display);

                // Add tool result to conversation
                bot.messages.push({
                    role: "tool",
                    content: display,
                    tool_call_id: toolCall.id,
                });
            }

            // Ask the model again with new tool results
            responseMessage = await bot.execute();
            continue;
        }

        // No more tool calls, return the final content (or a fallback summary)
        let content = responseMessage.content || "";
        if (!content && toolResultsAccum.length > 0) {
            const last = toolResultsAccum[toolResultsAccum.length - 1];
            content = "No final model answer was returned. Here's the latest Sequential Thinking result:\n" + last;
        }
        bot.messages.push({ role: "assistant", content });
        return content;
    }
}

async function interactiveLoop(): Promise<void> {
    // Main interactive loop for user queries
    console.log("=".repeat(80));
    console.log("INTERACTIVE CODING AGENT - Have a conversation!");
    console.log("Type your questions and press Enter. Use Ctrl+C to exit.");
    console.log("I can help with file operations, code analysis, and general questions.");
    console.log("Tips:");
    console.log("- Single-line: type and press Enter.");
    console.log("- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.");
    console.log("- Exit: type /exit or /quit.");
    console.log("=".repeat(80));

    const bot = new ChatBot(
        "You are a helpful coding assistant with access to file system tools. You can read files, write files, see directory structures, execute bash commands, and search in files. Use these tools to help with coding tasks and file management."
    );

    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });

    const askQuestion = (): Promise<string> => {
        return new Promise((resolve) => {
            rl.question("\nYour question: ", async (answer) => {
                const first = (answer ?? "").trim();
                if (first.toLowerCase() === "/ml") {
                    console.log("Enter multi-line input. Finish with /end or --- on a line by itself.");
                    const lines: string[] = [];
                    const askMore = (): void => {
                        rl.question("", (line) => {
                            const trimmed = (line ?? "").trim();
                            if (trimmed === "/end" || trimmed === "---") {
                                resolve(lines.join("\n").trim());
                            } else {
                                lines.push(line);
                                askMore();
                            }
                        });
                    };
                    askMore();
                } else {
                    resolve(first);
                }
            });
        });
    };

    try {
        while (true) {
            const userQuery = await askQuestion();
            // Allow quick exit commands
            if (userQuery.toLowerCase() === "/exit" || userQuery.toLowerCase() === "/quit") {
                console.log("Exiting...");
                break;
            }
            if (userQuery) { // Only process non-empty queries
                const result = await query(userQuery, bot);
                console.log(`Answer: ${result}`);
            } else {
                console.log("Please enter a question.");
            }
        }
    } catch (error) {
        console.log("\n\nGoodbye! Thanks for chatting!");
    } finally {
        rl.close();
    }

    // Handle Ctrl+C gracefully
    process.on('SIGINT', () => {
        console.log("\n\nGoodbye! Thanks for chatting!");
        rl.close();
        process.exit(0);
    });
}

// Main execution
async function main(): Promise<void> {
    // First show examples
    showExamples();
    
    // Then start interactive mode
    await interactiveLoop();
}

if (require.main === module) {
    main().catch(console.error);
}

export { ChatBot, query, showExamples, interactiveLoop };