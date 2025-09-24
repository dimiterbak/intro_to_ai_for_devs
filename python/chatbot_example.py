import os
from openai import OpenAI

# Make sure to set your environment variables accordingly
ENDPOINT = os.getenv("AI_ENDPOINT")
DEPLOYMENT_NAME = os.getenv("DEPLOYMENT_NAME")
API_KEY = os.getenv("AI_API_KEY")

class ChatBot:
    def __init__(self, system=""):
        self.number_of_prompts_sent = 0  # Initialize prompt counter
        self.system = system
        self.messages = []
        if self.system:
            self.messages.append({"role": "system", "content": system})

    def __call__(self, message):
        self.messages.append({"role": "user", "content": message})
        result = self.execute()
        self.messages.append({"role": "assistant", "content": result})
        return result

    def execute(self):
        self.number_of_prompts_sent += 1
        print("\n-- Prompt", self.number_of_prompts_sent, "--")
        print("\n=== FULL PROMPT SENT TO MODEL ===")  
        # Show the complete prompt being sent to the model
        for i, msg in enumerate(self.messages, 1):
            # print(f"Message {i} ({msg['role']}):")
            print(f"  {msg['content']}")
            print()

        client = OpenAI(
            base_url=f"{ENDPOINT}",
            api_key=API_KEY
        )

        response = client.chat.completions.create(
            model=DEPLOYMENT_NAME,
            messages=self.messages
        )
        # Uncomment this to print out token usage each time, e.g.
        # CompletionUsage(completion_tokens=27, prompt_tokens=225, total_tokens=252, completion_tokens_details=CompletionTokensDetails(accepted_prediction_tokens=0, audio_tokens=0, reasoning_tokens=0, rejected_prediction_tokens=0), prompt_tokens_details=PromptTokensDetails(audio_tokens=0, cached_tokens=0))
        # print(response.usage)
                
        print("=== END OF PROMPT ===\n")
        return response.choices[0].message.content
        

def show_examples():
    """Run some example queries to demonstrate the system"""
    print("=" * 80)
    print("CHATBOT EXAMPLES - Simple Q&A")
    print("=" * 80)
    
    examples = [
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "How many r's are in strawbery?",
        "Who was the first president of the United States?"
    ]
    
    for i, example in enumerate(examples, 1):
        print(f"\n--- Example {i} ---")
        print(example)


def query(question, bot=None): 
    result = bot(question)
    return result

def interactive_loop():
    # Main interactive loop for user queries
    print("=" * 80)
    print("INTERACTIVE CHATBOT - Have a conversation!")
    print("Type your questions and press Enter. Use Ctrl+C to exit.")
    print("=" * 80)
    
    bot = ChatBot()  # Create one bot instance to maintain conversation history

    try:
        while True:
            user_query = input("\nYour question: ").strip()
            if user_query:  # Only process non-empty queries
                result = query(user_query, bot)
                print(f"Answer: {result}")
            else:
                print("Please enter a question.")
    except KeyboardInterrupt:
        print("\n\nGoodbye! Thanks for chatting!")
    except EOFError:
        print("\n\nGoodbye! Thanks for chatting!")

if __name__ == '__main__':
    # First show examples
    show_examples()
    
    # Then start interactive mode
    interactive_loop()
