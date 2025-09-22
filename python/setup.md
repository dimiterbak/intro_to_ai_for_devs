# Setup 

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
uv pip install openai
```

## 3. Export your API key

Linux / macOS:
```
export OPENAI_API_KEY="your_api_key_here"
```

Windows PowerShell:
```
setx OPENAI_API_KEY "your_api_key_here"
```

## 4. Run a simple test

Create a test_openai.py file:

```
from openai import OpenAI

client = OpenAI()

response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "Hello, what is the capital of France?"}]
)

print(response.choices[0].message.content)
```

Run it:
```
python test_openai.py
```

If everything is set up correctly, you’ll see a reply from the model.
