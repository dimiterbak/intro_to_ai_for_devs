# Introduction to AI for Python Devs 

## Install uv

uv isn’t included with Python by default. You’ll need to install it first.

The recommended way is:
```
pip install uv
```
## 1. Create and activate a new uv environment
```
uv venv intro_to_ai
```

### on Linux/Mac
```
source intro_to_ai/bin/activate   
```

### or on Windows PowerShell:
```
 .\intro_to_ai\Scripts\activate
```

## 2. Install the OpenAI SDK
```
uv pip install openai httpx
```

## 3. Install MCP client for Sequential Thinking demo

You need Node.js with `npx` on your PATH and the MCP Python SDK:
```
brew install node   # macOS (or use your OS package manager)
uv pip install "mcp[cli]"
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

## 5. Run a simple test

The `test_openai.py` file is already created for you:

Run it:
```
python test_openai.py
```

If everything is set up correctly, you'll see a reply "The capital of France is Paris." from the model.

## 6. MCP smoke test

Verify the Sequential Thinking MCP server runs and responds:
```
python mcp_smoke_test.py
```
If it fails saying `npx` not found, install Node.js. If it says `mcp` not installed, run:
```
uv pip install "mcp[cli]"
```


