import os
import re
from pathlib import Path
from openai import OpenAI
import json
import asyncio
from typing import Any
from coding_agent_tools import CodingAgentTools
from types import SimpleNamespace

# Optional MCP client imports (installed via `mcp[cli]`)
try:
    from mcp import ClientSession, StdioServerParameters, types as mcp_types
    from mcp.client.stdio import stdio_client
    _MCP_AVAILABLE = True
except Exception:
    # Keep runtime-friendly: we'll raise a clear message when the tool is called
    _MCP_AVAILABLE = False

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
        defaultSystem = (
            "You are a helpful coding assistant with access to file system tools. "
            "Only use tools for file operations, code analysis, project navigation, or when the user explicitly asks you to run a shell command. "
            "Do NOT use tools for general knowledge questions; answer from your own knowledge unless the user specifically references files, paths, or commands."
        )
        self.system = system or defaultSystem
        self.messages = []
        if self.system:
            self.messages.append({"role": "system", "content": self.system})

    def get_tool_definitions(self):
        """Convert agent tools to OpenAI function calling format"""
        return [
            {
                "type": "function",
                "function": {
                    "name": "read_file",
                    "description": "Read and return the contents of a file at the given relative filepath. Prefer this over running shell commands like 'cat'.",
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
                    "description": "Execute a bash command in the shell and return its output, error, and exit code. Only use when the user explicitly asks to run a shell/terminal command. Prefer read_file/search_in_files for reading content.",
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
            ,
            {
                "type": "function",
                "function": {
                    "name": "sequentialthinking",
                    "description": "Facilitates a detailed, step-by-step thinking process for problem-solving and analysis using an MCP server.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "thought": {"type": "string", "description": "Your current thinking step."},
                            "nextThoughtNeeded": {"type": "boolean", "description": "Whether another thought step is needed."},
                            "thoughtNumber": {"type": "integer", "description": "Current thought number (1-based)."},
                            "totalThoughts": {"type": "integer", "description": "Estimated total thoughts needed."},
                            "isRevision": {"type": "boolean", "description": "Whether this revises previous thinking."},
                            "revisesThought": {"type": "integer", "description": "Which thought is being reconsidered."},
                            "branchFromThought": {"type": "integer", "description": "Branching point thought number."},
                            "branchId": {"type": "string", "description": "Branch identifier."},
                            "needsMoreThoughts": {"type": "boolean", "description": "If reaching end but realizing more thoughts are needed."}
                        },
                        "required": ["thought", "nextThoughtNeeded", "thoughtNumber", "totalThoughts"]
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
                # Only allow if the user explicitly asked to run a shell/terminal command
                last_user = None
                for msg in reversed(self.messages):
                    if msg.get("role") == "user":
                        last_user = msg
                        break

                text = (last_user.get("content") if last_user else "") or ""
                text = text.lower()
                explicit = re.search(r"(bash|shell|terminal|run|execute|npm|node|yarn|pnpm|pip|python|pytest|ts-node|tsc|make|gradle|cargo|go\s+run|go\s+build)", text)
                if not explicit:
                    # Mirror TS version: block unless explicitly requested
                    return {
                        "stdout": "",
                        "stderr": "Blocked: bash commands require explicit user request. Please proceed without execute_bash_command.",
                        "returncode": 1
                    }

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
            elif tool_name == "sequentialthinking":
                return self._call_sequential_thinking(arguments)
            else:
                return f"Unknown tool: {tool_name}"
        except Exception as e:
            return f"Error executing {tool_name}: {str(e)}"

    def _call_sequential_thinking(self, args: dict[str, Any]):
        """Bridge call to the sequentialthinking MCP server via stdio.

        This launches the MCP server using npx and calls its 'sequentialthinking' tool.
        Returns either structured content (dict) or a text summary string.
        """
        if not _MCP_AVAILABLE:
            return (
                "MCP client not installed. Please install the Python SDK with: "
                "uv pip install \"mcp[cli]\" (or pip install \"mcp[cli]\"). "
                "Also ensure Node.js and npx are installed."
            )

        async def _run() -> Any:
            server_params = StdioServerParameters(
                command="npx",
                args=["-y", "@modelcontextprotocol/server-sequential-thinking"],
                env={
                    # Allow turning off thought logging for classroom privacy if desired
                    "DISABLE_THOUGHT_LOGGING": os.environ.get("DISABLE_THOUGHT_LOGGING", "false"),
                },
            )

            try:
                async with stdio_client(server_params) as (read, write):
                    async with ClientSession(read, write) as session:
                        await session.initialize()

                        # Resolve tool name from server by normalization (handles underscore/hyphen/no-separator)
                        def _norm(name: str) -> str:
                            return name.replace("_", "").replace("-", "").lower()

                        desired_norm = _norm("sequentialthinking")
                        chosen_name = "sequentialthinking"  # default
                        candidates = ["sequentialthinking", "sequential-thinking", "sequential_thinking"]
                        try:
                            tools = await session.list_tools()
                            available = [t.name for t in tools.tools]
                            # Prefer exact normalized match from server list
                            match = next((n for n in available if _norm(n) == desired_norm), None)
                            if match:
                                chosen_name = match
                            else:
                                # append server-provided names as fallbacks too
                                candidates.extend([n for n in available if n not in candidates])
                        except Exception:
                            # If listing fails, proceed with default name
                            pass

                        # Try calling, fallback through candidates
                        last_err: Exception | None = None
                        for name in [chosen_name] + [c for c in candidates if c != chosen_name]:
                            try:
                                result = await session.call_tool(name, arguments=args)
                                # If we got a textual unknown-tool response, continue loop
                                try:
                                    texts_tmp = []
                                    for c in result.content:
                                        if isinstance(c, mcp_types.TextContent):
                                            texts_tmp.append(c.text)
                                    joined = "\n".join(texts_tmp)
                                    if "Unknown tool:" in joined:
                                        last_err = RuntimeError(joined)
                                        continue
                                except Exception:
                                    pass
                                # success
                                chosen_name = name
                                break
                            except Exception as e:
                                last_err = e
                                continue
                        else:
                            # Exhausted candidates
                            raise last_err or RuntimeError("Failed to call sequential thinking tool.")
                        # At this point 'result' is successful

                        # Prefer structured content if provided
                        structured = getattr(result, "structuredContent", None)
                        if structured:
                            return structured

                        # Fallback to concatenating text content
                        texts = []
                        for content in result.content:
                            if isinstance(content, mcp_types.TextContent):
                                texts.append(content.text)
                            else:
                                # Best-effort stringify
                                try:
                                    texts.append(json.dumps(content.__dict__, default=str))
                                except Exception:
                                    texts.append(str(content))
                        return "\n".join(texts) if texts else "(no content)"
            except FileNotFoundError:
                return (
                    "Failed to launch MCP server via npx. Ensure Node.js and npx are installed "
                    "and available in PATH. Try: brew install node"
                )
            except Exception as e:
                return f"Error calling sequentialthinking via MCP: {e}"

        # Run the async client
        return asyncio.run(_run())

    def should_enable_tools_for_next_turn(self) -> bool:
        """Heuristic: expose tools only if the last user message suggests file/shell intent.

        Examples that SHOULD enable tools: mentions of reading/writing/creating files, searching, grep,
        file tree, directories/paths, opening files, running shell/CLI commands, common package managers,
        programming file extensions, or path-like tokens.
        """
        # Find the last user message
        last_user = None
        for msg in reversed(self.messages):
            if msg.get("role") == "user":
                last_user = msg
                break

        # If there's no user message yet, allow tools (safe default)
        if not last_user:
            return True

        text = (last_user.get("content") or "").lower()
        tool_intents = [
            "read file", "write file", "create file", "append to file", "search", "grep", "file tree",
            "directory", "path", "open", "bash", "shell", "terminal", "run", "execute", "npm", "node",
            "ts-node", "tsc", "yarn", "pnpm", "pip", "python", "pytest", "make", "gradle", "cargo", "go run", "go build", ".ts", ".js", ".json",
            ".py", ".md", "/", "\\"
        ]
        return any(w in text for w in tool_intents)

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

        try:
            # Conditionally include tools in the API call
            include_tools = self.should_enable_tools_for_next_turn()
            if include_tools:
                response = client.chat.completions.create(
                    model=DEPLOYMENT_NAME,
                    messages=self.messages,
                    tools=self.get_tool_definitions()
                )
            else:
                response = client.chat.completions.create(
                    model=DEPLOYMENT_NAME,
                    messages=self.messages
                )
            print("=== END OF PROMPT ===\n")
            return response.choices[0].message
        except Exception as e:
            # Catch and print any error from the API call instead of crashing
            print("=== END OF PROMPT ===\n")
            print(f"Error during model call: {e}")
            # Return a minimal message-like object to keep the flow working
            return SimpleNamespace(content=f"Error: {e}", tool_calls=None)
        

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

    # Keep handling tool calls until the model returns a final answer
    max_tool_cycles = 8
    cycles = 0
    tool_results_accum: list[str] = []

    while True:
        if response_message.tool_calls:
            cycles += 1
            if cycles > max_tool_cycles:
                break

            # Add the assistant's response to conversation history
            bot.messages.append({
                "role": "assistant",
                # content may be None when tool-calling
                "content": response_message.content or "",
                "tool_calls": response_message.tool_calls,
            })

            # Execute each tool call
            for tool_call in response_message.tool_calls:
                function_name = tool_call.function.name
                function_args = json.loads(tool_call.function.arguments)

                print(f"🔧 Executing tool: {function_name} with args: {function_args}")

                # Execute the tool
                tool_result = bot.execute_tool(function_name, function_args)

                # Keep a readable transcript of tool outputs
                display = tool_result if isinstance(tool_result, str) else json.dumps(tool_result, indent=2)
                tool_results_accum.append(display)

                # Add tool result to conversation
                bot.messages.append({
                    "role": "tool",
                    "content": display,
                    "tool_call_id": tool_call.id,
                })

            # Ask the model again with new tool results
            response_message = bot.execute()
            continue

        # No more tool calls, return the final content (or a fallback summary)
        content = response_message.content or ""
        if not content and tool_results_accum:
            # Fallback summary if the model didn't produce a final message
            last = tool_results_accum[-1]
            content = (
                "No final model answer was returned. Here's the latest Sequential Thinking result:\n" 
                + (last if isinstance(last, str) else json.dumps(last))
            )
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

    bot = ChatBot() # Initialize bot with default system prompt
    
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
