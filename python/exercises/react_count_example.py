"""
This notebook demonstrates the setup and use of the ReAct prompting framework with OpenAI's API, following the implementation details from 
[Simon Willison's learning log](https://til.simonwillison.net/llms/python-react-pattern).
The ReAct pattern combines reasoning and acting, allowing the model to iteratively think about a problem, take actions (like looking up information), and refine its answers based on observations.
"""

import os
from openai import OpenAI
import re
import httpx


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

prompt = """
You run in a loop of Thought, Action, PAUSE, Observation.
At the end of the loop you output an Answer
Use Thought to describe your thoughts about the question you have been asked.
Use Action to run one of the actions available to you - then return PAUSE.
Observation will be the result of running those actions.

Your available actions are:

calculate:
e.g. calculate: 4 * 7 / 3
Runs a calculation and returns the number - uses Python so be sure to use floating point syntax if necessary

wikipedia:
e.g. wikipedia: Django
Returns a summary from searching Wikipedia

count_letters:
e.g. count_letters: letter r in word strawberry
Counts how many times a specific letter appears in a word (case-insensitive)

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
""".strip()

def query(question, bot=None, max_turns=5):
    print("\nQuestion:", question)
    action_re = re.compile('^Action: (\w+): (.*)$')
    i = 0
    next_prompt = question
    while i < max_turns:
        result = bot(next_prompt)
        print(result)
        actions = [action_re.match(a) for a in result.split('\n') if action_re.match(a)]
        if actions:
            # There is an action to run
            action, action_input = actions[0].groups()
            if action not in known_actions:
                raise Exception("Unknown action: {}: {}".format(action, action_input))
            print(" -- running {} {}".format(action, action_input))
            observation = known_actions[action](action_input)
            print("Observation:", observation)
            next_prompt = "Observation: {}".format(observation)
        else:
            return

def wikipedia(q):
    return httpx.get("https://en.wikipedia.org/w/api.php", params={
        "action": "query",
        "list": "search",
        "srsearch": q,
        "format": "json"
    }).json()["query"]["search"][0]["snippet"]


def calculate(what):
    return eval(what)

def count_letters(query):
    """
    Count occurrences of a letter in a word.
    Expected format: "letter X in word Y" or "X in Y"
    """
    # Parse the query to extract letter and word
    query = query.lower().strip()
    
    # Try different parsing patterns
    patterns = [
        r'letter\s+(\w)\s+in\s+word\s+(\w+)',
        r'letter\s+(\w)\s+in\s+(\w+)',
        r'(\w)\s+in\s+word\s+(\w+)',
        r'(\w)\s+in\s+(\w+)'
    ]
    
    for pattern in patterns:
        match = re.match(pattern, query)
        if match:
            letter, word = match.groups()
            count = word.count(letter)
            return f"The letter '{letter}' appears {count} time(s) in the word '{word}'"
    
    return f"Could not parse the query '{query}'. Please use format like 'letter r in word strawberry' or 'r in strawberry'"

known_actions = {
    "wikipedia": wikipedia,
    "calculate": calculate,
    "count_letters": count_letters
}

def show_examples():
    """Run some example queries to demonstrate the system"""
    print("=" * 80)
    print("QUERY EXAMPLES - Demonstrating ReAct (Reason + Act) Pattern")
    print("=" * 80)
    
    examples = [
        "What states does Ohio share borders with?",
        "Calculate the square root of 256.",
        "Who was the first president of the United States?",
        "How many r's are in strawberry?"
    ]
    
    for i, example in enumerate(examples, 1):
        print(f"\n--- Example {i} ---")
        print(example)

def interactive_loop():
    """Main interactive loop for user queries"""
    print("=" * 80)
    print("INTERACTIVE MODE - Ask your own questions!")
    print("Type your questions and press Enter. Use Ctrl+C to exit.")
    print("=" * 80)
    
    bot = ChatBot(prompt)  # Create one bot instance to maintain conversation history
    try:
        while True:
            user_query = input("\nYour question: ").strip()
            if user_query:  # Only process non-empty queries
                print()
                query(user_query, bot)
            else:
                print("Please enter a question.")
    except KeyboardInterrupt:
        print("\n\nGoodbye! Thanks for using the ReAct agent.")
    except EOFError:
        print("\n\nGoodbye! Thanks for using the ReAct agent.")

if __name__ == '__main__':
    # First show examples
    show_examples()
    
    # Then start interactive mode
    interactive_loop()
