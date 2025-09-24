import os
from openai import OpenAI

# Make sure to set your environment variables accordingly
ENDPOINT = os.getenv("AI_ENDPOINT")
DEPLOYMENT_NAME = os.getenv("DEPLOYMENT_NAME")
API_KEY = os.getenv("AI_API_KEY")

client = OpenAI(
    base_url=f"{ENDPOINT}",
    api_key=API_KEY
)

response = client.chat.completions.create(
    model=DEPLOYMENT_NAME,
    messages=[{"role": "user", "content": "Hello, what is the capital of France?"}]
)

print(response.choices[0].message.content)