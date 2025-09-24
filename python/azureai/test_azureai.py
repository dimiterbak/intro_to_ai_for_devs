import os
from openai import OpenAI

# Make sure to set your environment variables accordingly
# AZURE_AI_ENDPOINT
endpoint = os.getenv("AZURE_AI_ENDPOINT")
model_name = "gpt-35-turbo"
deployment_name = "gpt-35-turbo"

# set your environment variables accordingly
# AZURE_AI_API_KEY
api_key = os.getenv("AZURE_AI_API_KEY")

client = OpenAI(
    base_url=f"{endpoint}",
    api_key=api_key
)

response = client.chat.completions.create(
    model=deployment_name,
    messages=[{"role": "user", "content": "Hello, what is the capital of France?"}]
)

print(response.choices[0].message.content)