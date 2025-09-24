import os
from openai import OpenAI

endpoint = "https://icb-genai-coe.cognitiveservices.azure.com/openai/v1/"
model_name = "gpt-35-turbo"
deployment_name = "gpt-35-turbo"

# set your environment variables accordingly
# AZURE_OPENAI_API_KEY
api_key = os.getenv("AZURE_OPENAI_API_KEY")

client = OpenAI(
    base_url=f"{endpoint}",
    api_key=api_key
)

response = client.chat.completions.create(
    model=deployment_name,
    messages=[{"role": "user", "content": "Hello, what is the capital of France?"}]
)

print(response.choices[0].message.content)