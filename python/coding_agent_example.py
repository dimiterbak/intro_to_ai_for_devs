import os
from pathlib import Path
from openai import OpenAI
import json
from coding_agent_tools import CodingAgentTools

# Make sure to set your environment variables accordingly
ENDPOINT = os.getenv("AI_ENDPOINT")
DEPLOYMENT_NAME = os.getenv("DEPLOYMENT_NAME")
API_KEY = os.getenv("AI_API_KEY")

class ChatBot:
    def __init__(self, system="", project_path=None):
        self.number_of_prompts_sent = 0  # Initialize prompt counter
        if project_path is None:
            project_path = Path.cwd()  # Use current working directory as default
        self.agent_tools = CodingAgentTools(Path(project_path))
        self.system = system
        self.messages = []
        if self.system:
            self.messages.append({"role": "system", "content": system})

    def get_tool_definitions(self):
        """Convert agent tools to OpenAI function calling format"""
        return [
            {
                "type": "function",
                "function": {
                    "name": "read_file",
                    "description": "Read and return the contents of a file at the given relative filepath",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "filepath": {
                                "type": "string",
                                "description": "Path to the file, relative to the project directory"
                            }
                        },
                        "required": ["filepath"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "write_file",
                    "description": "Write content to a file at the given relative filepath, creating directories as needed",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "filepath": {
                                "type": "string",
                                "description": "Path to the file, relative to the project directory"
                            },
                            "content": {
                                "type": "string",
                                "description": "Content to write to the file"
                            }
                        },
                        "required": ["filepath", "content"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "see_file_tree",
                    "description": "Return a list of all files and directories under the given root directory",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "root_dir": {
                                "type": "string",
                                "description": "Root directory to list from, relative to the project directory",
                                "default": "."
                            }
                        },
                        "required": []
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "execute_bash_command",
                    "description": "Execute a bash command in the shell and return its output, error, and exit code",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "command": {
                                "type": "string",
                                "description": "The bash command to execute"
                            },
                            "cwd": {
                                "type": "string",
                                "description": "Working directory to run the command in, relative to the project directory"
                            }
                        },
                        "required": ["command"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "search_in_files",
                    "description": "Search for a pattern in all files under the given root directory",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "pattern": {
                                "type": "string",
                                "description": "Pattern to search for in files"
                            },
                            "root_dir": {
                                "type": "string",
                                "description": "Root directory to search from, relative to the project directory",
                                "default": "."
                            }
                        },
                        "required": ["pattern"]
                    }
                }
            }
        ]

    def execute_tool(self, tool_name, arguments):
        """Execute a tool function with the given arguments"""
        try:
            if tool_name == "read_file":
                return self.agent_tools.read_file(arguments["filepath"])
            elif tool_name == "write_file":
                self.agent_tools.write_file(arguments["filepath"], arguments["content"])
                return f"Successfully wrote to {arguments['filepath']}"
            elif tool_name == "see_file_tree":
                root_dir = arguments.get("root_dir", ".")
                return self.agent_tools.see_file_tree(root_dir)
            elif tool_name == "execute_bash_command":
                command = arguments["command"]
                cwd = arguments.get("cwd")
                stdout, stderr, returncode = self.agent_tools.execute_bash_command(command, cwd)
                return {
                    "stdout": stdout,
                    "stderr": stderr,
                    "returncode": returncode
                }
            elif tool_name == "search_in_files":
                pattern = arguments["pattern"]
                root_dir = arguments.get("root_dir", ".")
                return self.agent_tools.search_in_files(pattern, root_dir)
            else:
                return f"Unknown tool: {tool_name}"
        except Exception as e:
            return f"Error executing {tool_name}: {str(e)}"

    def __call__(self, message):
        self.messages.append({"role": "user", "content": message})
        response_message = self.execute()
        
        # Return the response message object for query() to handle tool calling
        return response_message

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

        # Include tools in the API call
        response = client.chat.completions.create(
            model=DEPLOYMENT_NAME,
            messages=self.messages,
            tools=self.get_tool_definitions()
        )
        
        print("=== END OF PROMPT ===\n")
        return response.choices[0].message
        

def show_examples():
    """Run some example queries to demonstrate the system"""
    print("=" * 80)
    print("CHATBOT EXAMPLES - Tool-Enabled Coding Agent")
    print("=" * 80)
    
    examples = [
        "Show me the file structure of this project",
        "Read the README.md file",
        "Search for any Python files that contain 'OpenAI'",
        "What states does Ohio share borders with?",  # Regular question without tools
        "Create a simple hello world Python script in a new file called hello.py"
    ]
    
    bot = ChatBot(system="You are a helpful coding assistant with access to file system tools. Use the available tools to help with coding tasks.")
    
    for i, example in enumerate(examples, 1):
        print(f"\n--- Example {i} ---")
        print(f"{example}")


def query(question, bot):

    response_message = bot(question)
    
    # Check if the model wants to call a tool
    if response_message.tool_calls:
        # Add the assistant's response to conversation history
        bot.messages.append({
            "role": "assistant", 
            "content": response_message.content,
            "tool_calls": response_message.tool_calls
        })
        
        # Execute each tool call
        for tool_call in response_message.tool_calls:
            function_name = tool_call.function.name
            function_args = json.loads(tool_call.function.arguments)
            
            print(f"🔧 Executing tool: {function_name} with args: {function_args}")
            
            # Execute the tool
            tool_result = bot.execute_tool(function_name, function_args)
            
            # Add tool result to conversation
            bot.messages.append({
                "role": "tool",
                "content": json.dumps(tool_result) if not isinstance(tool_result, str) else tool_result,
                "tool_call_id": tool_call.id
            })
        
        # Get final response from the model after tool execution
        final_response = bot.execute()
        
        final_content = final_response.content
        bot.messages.append({"role": "assistant", "content": final_content})
        return final_content
    
    # No tool calls, return the response directly
    content = response_message.content
    bot.messages.append({"role": "assistant", "content": content})
    return content

def interactive_loop():
    # Main interactive loop for user queries
    print("=" * 80)
    print("INTERACTIVE CODING AGENT - Have a conversation!")
    print("Type your questions and press Enter. Use Ctrl+C to exit.")
    print("I can help with file operations, code analysis, and general questions.")
    print()
    print("Tips:")
    print("- Single-line: type and press Enter.")
    print("- Multi-line: type /ml and press Enter, then paste lines; finish with /end (or ---) on its own line.")
    print("=" * 80)

    bot = ChatBot(system="You are a helpful coding assistant with access to file system tools. You can read files, write files, see directory structures, execute bash commands, and search in files. Use these tools to help with coding tasks and file management.")  # Create one bot instance to maintain conversation history
    
    i = 0  # Prompt counter
    try:
        while True:
            user_query = input("\nYour question: ").strip()

            # Enter multi-line mode if requested
            if user_query.lower() == "/ml":
                print("Enter multi-line input. Finish with /end or --- on a line by itself.")
                lines: list[str] = []
                while True:
                    try:
                        line = input()
                    except EOFError:
                        # Treat EOF as end of multi-line input
                        break
                    if line.strip() in {"/end", "---"}:
                        break
                    lines.append(line)
                user_query = "\n".join(lines).strip()

            # Allow quick exit commands
            if user_query.lower() in {"/exit", "/quit"}:
                print("Exiting...")
                break

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
