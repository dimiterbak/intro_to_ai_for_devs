import OpenAI from 'openai';

// Make sure to set your environment variables accordingly
const ENDPOINT = process.env.AI_ENDPOINT;
const DEPLOYMENT_NAME = process.env.DEPLOYMENT_NAME;
const API_KEY = process.env.AI_API_KEY;

// Validate required environment variables
if (!API_KEY) {
    console.error('Error: AI_API_KEY environment variable is not set');
    process.exit(1);
}

if (!ENDPOINT) {
    console.error('Error: AI_ENDPOINT environment variable is not set');
    process.exit(1);
}

if (!DEPLOYMENT_NAME) {
    console.error('Error: DEPLOYMENT_NAME environment variable is not set');
    process.exit(1);
}

const client = new OpenAI({
    baseURL: ENDPOINT,
    apiKey: API_KEY
});

async function testOpenAI() {
    try {
        const response = await client.chat.completions.create({
            model: DEPLOYMENT_NAME!,
            messages: [{ role: "user", content: "Hello, what is the capital of France?" }]
        });

        console.log(response.choices[0].message.content);
    } catch (error) {
        console.error('Error calling OpenAI:', error);
    }
}

testOpenAI();