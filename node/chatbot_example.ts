import OpenAI from 'openai';
import * as readline from 'readline';

// Make sure to set your environment variables accordingly
const ENDPOINT = process.env.AI_ENDPOINT;
const DEPLOYMENT_NAME = process.env.DEPLOYMENT_NAME;
const API_KEY = process.env.AI_API_KEY;

interface Message {
    role: 'system' | 'user' | 'assistant';
    content: string;
}

class ChatBot {
    private numberOfPromptsSent: number = 0;
    private system: string;
    private messages: Message[] = [];
    private client: OpenAI;

    constructor(system: string = "") {
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

    async call(message: string): Promise<string> {
        this.messages.push({ role: "user", content: message });
        const result = await this.execute();
        this.messages.push({ role: "assistant", content: result });
        return result;
    }

    private async execute(): Promise<string> {
        this.numberOfPromptsSent++;
        console.log(`\n-- Prompt ${this.numberOfPromptsSent} --`);
        console.log("\n=== FULL PROMPT SENT TO MODEL ===");
        
        // Show the complete prompt being sent to the model
        this.messages.forEach((msg, i) => {
            console.log(`  ${msg.content}`);
            console.log();
        });

        const response = await this.client.chat.completions.create({
            model: DEPLOYMENT_NAME!,
            messages: this.messages
        });

        // Uncomment this to print out token usage each time
        // console.log(response.usage);
        
        console.log("=== END OF PROMPT ===\n");
        return response.choices[0].message.content || "";
    }
}

function showExamples(): void {
    console.log("=".repeat(80));
    console.log("CHATBOT EXAMPLES - Simple Q&A");
    console.log("=".repeat(80));
    
    const examples = [
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "How many r's are in strawbery?",
        "Who was the first president of the United States?"
    ];
    
    examples.forEach((example, i) => {
        console.log(`\n--- Example ${i + 1} ---`);
        console.log(example);
    });
}

async function query(question: string, bot: ChatBot): Promise<string> {
    const result = await bot.call(question);
    return result;
}

async function interactiveLoop(): Promise<void> {
    // Main interactive loop for user queries
    console.log("=".repeat(80));
    console.log("INTERACTIVE CHATBOT - Have a conversation!");
    console.log("Type your questions and press Enter. Use Ctrl+C to exit.");
    console.log("=".repeat(80));
    
    const bot = new ChatBot(); // Create one bot instance to maintain conversation history

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