# Setup for Node.js TypeScript

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
- `typescript` - TypeScript compiler
- `ts-node` - TypeScript execution environment
- `@types/node` - Node.js type definitions

## 3. Export your environment variables

Linux / macOS:
```
export AI_API_KEY="your_api_key_here"
export AI_ENDPOINT="your_endpint_key_here"
export DEPLOYMENT_NAME = "your_model_name-here"
```

Windows PowerShell:
```
setx AI_API_KEY "your_api_key_here"
setx AI_ENDPOINT "your_endpint_key_here"
setx DEPLOYMENT_NAME "your_model_name-here"
```

## 4. Run a simple test

The `test_openai.ts` file is already created for you:

### Run it with ts-node:
```bash
npm run dev
```

If everything is set up correctly, you'll see a reply "The capital of France is Paris." from the model.

## Available Scripts

- `npm run dev` - Run TypeScript files directly with ts-node
- `npm run build` - Compile TypeScript to JavaScript
- `npm start` - Run the compiled JavaScript
- `npm test` - Build and run the project

## Troubleshooting

1. **Module not found errors**: Make sure you've run `npm install`
2. **Environment variable issues**: Double-check that your environment variables are set correctly
3. **TypeScript compilation errors**: Check that your `tsconfig.json` is properly configured
4. **API errors**: Verify your API key, endpoint, and deployment name are correct