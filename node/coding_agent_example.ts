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
            }
        ];
    }

    /**
     * Execute a tool function with the given arguments
     */
    executeTool(toolName: string, args: Record<string, any>): string | CommandResult | string[] | SearchMatch[] {
        try {
            switch (toolName) {
                case "read_file":
                    return this.agentTools.readFile(args.filepath);
                    
                case "write_file":
                    this.agentTools.writeFile(args.filepath, args.content);
                    return `Successfully wrote to ${args.filepath}`;
                    
                case "see_file_tree":
                    const rootDir = args.root_dir || ".";
                    return this.agentTools.seeFileTree(rootDir);
                    
                case "execute_bash_command":
                    const command = args.command;
                    const cwd = args.cwd;
                    return this.agentTools.executeBashCommand(command, cwd);
                    
                case "search_in_files":
                    const pattern = args.pattern;
                    const searchRootDir = args.root_dir || ".";
                    return this.agentTools.searchInFiles(pattern, searchRootDir);
                    
                default:
                    return `Unknown tool: ${toolName}`;
            }
        } catch (error) {
            return `Error executing ${toolName}: ${error instanceof Error ? error.message : String(error)}`;
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
        console.log(`Question: ${examples[i]}`);
    }
}

async function query(question: string, bot: ChatBot): Promise<string> {
    const responseMessage = await bot.call(question);
    
    // Check if the model wants to call a tool
    if (responseMessage.tool_calls && responseMessage.tool_calls.length > 0) {
        // Add the assistant's response to conversation history
        bot.messages.push({
            role: "assistant",
            content: responseMessage.content || "",
            tool_calls: responseMessage.tool_calls
        });
        
        // Execute each tool call
        for (const toolCall of responseMessage.tool_calls) {
            const functionName = toolCall.function.name;
            const functionArgs = JSON.parse(toolCall.function.arguments);
            
            console.log(`🔧 Executing tool: ${functionName} with args:`, functionArgs);
            
            // Execute the tool
            const toolResult = bot.executeTool(functionName, functionArgs);
            
            // Add tool result to conversation
            bot.messages.push({
                role: "tool",
                content: typeof toolResult === 'string' ? toolResult : JSON.stringify(toolResult),
                tool_call_id: toolCall.id
            });
        }
        
        // Get final response from the model after tool execution
        const finalResponse = await bot.execute();
        
        const finalContent = finalResponse.content || "";
        bot.messages.push({ role: "assistant", content: finalContent });
        return finalContent;
    }
    
    // No tool calls, return the response directly
    const content = responseMessage.content || "";
    bot.messages.push({ role: "assistant", content });
    return content;
}

async function interactiveLoop(): Promise<void> {
    // Main interactive loop for user queries
    console.log("=".repeat(80));
    console.log("INTERACTIVE CODING AGENT - Have a conversation!");
    console.log("Type your questions and press Enter. Use Ctrl+C to exit.");
    console.log("I can help with file operations, code analysis, and general questions.");
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
            rl.question("\nYour question: ", (answer) => {
                resolve(answer.trim());
            });
        });
    };

    try {
        while (true) {
            const userQuery = await askQuestion();
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