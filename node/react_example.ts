/**
 * This file demonstrates the setup and use of the ReAct prompting framework with OpenAI's API, following the implementation details from 
 * Simon Willison's learning log (https://til.simonwillison.net/llms/python-react-pattern).
 * The ReAct pattern combines reasoning and acting, allowing the model to iteratively think about a problem, take actions (like looking up information), and refine its answers based on observations.
 */

import OpenAI from 'openai';
import axios from 'axios';
import * as readlineSync from 'readline-sync';

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

    constructor(system: string = '') {
        this.system = system;
        if (this.system) {
            this.messages.push({ role: 'system', content: system });
        }
    }

    async call(message: string): Promise<string> {
        this.messages.push({ role: 'user', content: message });
        const result = await this.execute();
        this.messages.push({ role: 'assistant', content: result });
        return result;
    }

    private async execute(): Promise<string> {
        this.numberOfPromptsSent += 1;
        console.log(`\n-- Prompt ${this.numberOfPromptsSent} --`);
        console.log('\n=== FULL PROMPT SENT TO MODEL ===');
        
        // Show the complete prompt being sent to the model
        this.messages.forEach((msg, i) => {
            console.log(`  ${msg.content}`);
            console.log();
        });

        if (!ENDPOINT || !API_KEY || !DEPLOYMENT_NAME) {
            throw new Error('Missing required environment variables: AI_ENDPOINT, AI_API_KEY, or DEPLOYMENT_NAME');
        }

        const client = new OpenAI({
            baseURL: ENDPOINT,
            apiKey: API_KEY
        });

        const response = await client.chat.completions.create({
            model: DEPLOYMENT_NAME,
            messages: this.messages
        });

        // Uncomment this to print out token usage each time
        // console.log(response.usage);

        console.log('=== END OF PROMPT ===\n');
        return response.choices[0].message.content || '';
    }
}

const prompt = `
You run in a loop of Thought, Action, PAUSE, Observation.
At the end of the loop you output an Answer
Use Thought to describe your thoughts about the question you have been asked.
Use Action to run one of the actions available to you - then return PAUSE.
Observation will be the result of running those actions.

Your available actions are:

calculate:
e.g. calculate: 4 * 7 / 3
Runs a calculation and returns the number - uses JavaScript so be sure to use floating point syntax if necessary

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

Answer: The capital of France is Paris
`.trim();

async function query(question: string, bot: ChatBot, maxTurns: number = 5): Promise<void> {
    console.log('\nQuestion:', question);
    const actionRegex = /^Action: (\w+): (.*)$/;
    let i = 0;
    let nextPrompt = question;
    
    while (i < maxTurns) {
        const result = await bot.call(nextPrompt);
        console.log(result);
        
        const lines = result.split('\n');
        const actions = lines.map(line => line.match(actionRegex)).filter(Boolean);
        
        if (actions.length > 0 && actions[0]) {
            // There is an action to run
            const [, action, actionInput] = actions[0];
            if (!(action in knownActions)) {
                throw new Error(`Unknown action: ${action}: ${actionInput}`);
            }
            console.log(` -- running ${action} ${actionInput}`);
            const observation = await knownActions[action](actionInput);
            console.log('Observation:', observation);
            nextPrompt = `Observation: ${observation}`;
        } else {
            return;
        }
        i++;
    }
}

async function wikipedia(q: string): Promise<string> {
    try {
        const response = await axios.get('https://en.wikipedia.org/w/api.php', {
            params: {
                action: 'query',
                list: 'search',
                srsearch: q,
                format: 'json'
            }
        });
        return response.data.query.search[0]?.snippet || 'No results found';
    } catch (error) {
        return `Error searching Wikipedia: ${error}`;
    }
}

function calculate(what: string): string {
    try {
        // Note: Using eval is dangerous in production code. In a real application,
        // you should use a proper math expression parser like math.js
        const result = eval(what);
        return result.toString();
    } catch (error) {
        return `Error calculating: ${error}`;
    }
}

const knownActions: { [key: string]: (input: string) => Promise<string> | string } = {
    wikipedia,
    calculate
};

function showExamples(): void {
    console.log('='.repeat(80));
    console.log('QUERY EXAMPLES - Demonstrating ReAct (Reason + Act) Pattern');
    console.log('='.repeat(80));
    
    const examples = [
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "Who was the first president of the United States?",
        "How many r's are in strawberry?"
    ];
    
    examples.forEach((example, i) => {
        console.log(`\n--- Example ${i + 1} ---`);
        console.log(example);
    });
}

async function interactiveLoop(): Promise<void> {
    console.log('='.repeat(80));
    console.log('INTERACTIVE MODE - Ask your own questions!');
    console.log('Type your questions and press Enter. Type "exit" or use Ctrl+C to exit.');
    console.log('='.repeat(80));
    
    const bot = new ChatBot(prompt); // Create one bot instance to maintain conversation history
    
    try {
        while (true) {
            const userQuery = readlineSync.question('\nYour question: ').trim();
            
            if (userQuery.toLowerCase() === 'exit') {
                break;
            }
            
            if (userQuery) { // Only process non-empty queries
                console.log();
                await query(userQuery, bot);
            } else {
                console.log('Please enter a question.');
            }
        }
    } catch (error) {
        if (error instanceof Error && error.message.includes('SIGINT')) {
            console.log('\n\nGoodbye! Thanks for using the ReAct agent.');
        } else {
            console.error('An error occurred:', error);
        }
    }
    
    console.log('\n\nGoodbye! Thanks for using the ReAct agent.');
}

// Main execution
async function main(): Promise<void> {
    try {
        // First show examples
        showExamples();
        
        // Then start interactive mode
        await interactiveLoop();
    } catch (error) {
        console.error('Error running ReAct example:', error);
        process.exit(1);
    }
}

// Only run main if this file is executed directly
if (require.main === module) {
    main();
}

export { ChatBot, query, wikipedia, calculate, knownActions };