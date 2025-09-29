import os
import sys
from openai import OpenAI

# Make sure to set your environment variables accordingly
ENDPOINT = os.getenv("AI_ENDPOINT")
DEPLOYMENT_NAME = os.getenv("DEPLOYMENT_NAME")
API_KEY = os.getenv("AI_API_KEY")

# Validate required environment variables
if not API_KEY:
    print('Error: AI_API_KEY environment variable is not set', file=sys.stderr)
    sys.exit(1)

if not ENDPOINT:
    print('Error: AI_ENDPOINT environment variable is not set', file=sys.stderr)
    sys.exit(1)

if not DEPLOYMENT_NAME:
    print('Error: DEPLOYMENT_NAME environment variable is not set', file=sys.stderr)
    sys.exit(1)

client = OpenAI(
    base_url=ENDPOINT,
    api_key=API_KEY
)

response = client.chat.completions.create(
    model=DEPLOYMENT_NAME,
    messages=[{"role": "user", "content": "Hello, what is the capital of France?"}]
)

print(response.choices[0].message.content)