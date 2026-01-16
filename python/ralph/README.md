# Ralph - Autonomous AI Agent

> **Note**: This implementation is based on the [Ralph repository](https://github.com/snarktank/ralph) by [Ryan Carson](https://x.com/ryancarson). The original Ralph is an autonomous AI agent loop that runs Amp repeatedly until all PRD items are complete. This Python implementation adapts the Ralph pattern for use with OpenAI-compatible APIs and the coding agent framework.

Ralph is an autonomous AI agent loop that runs the coding agent repeatedly until all PRD items are complete. Each iteration is a fresh instance with clean context.

## Prerequisites

- Python 3.11+ with required dependencies (see main `python/README.md`)
- Environment variables set:
  - `AI_ENDPOINT`
  - `DEPLOYMENT_NAME`
  - `AI_API_KEY`
- `jq` installed (for `ralph.sh` to parse JSON)

## Running `coding_agent_ralph_example.py`

The script supports three modes of operation:

### 1. Interactive Mode (Default)

Run without arguments and no stdin input to start an interactive conversation:

```bash
python ralph/coding_agent_ralph_example.py
```

**Features:**
- Multi-line input: Type `/ml` and press Enter, then paste multiple lines. Finish with `/end` or `---` on its own line.
- Exit commands: Type `/exit` or `/quit` to exit
- Use Ctrl+C to interrupt

**Example:**
```
Your question: /ml
Enter multi-line input. Finish with /end or --- on a line by itself.
What files are in the current directory?
Can you read the README?
/end
```

### 2. Command Line Argument Mode

Pass your question directly as command line arguments:

```bash
python ralph/coding_agent_ralph_example.py "What files are in this directory?"
```

**Multi-word questions:**
```bash
# With quotes (recommended)
python ralph/coding_agent_ralph_example.py "Read the README file and summarize it"

# Without quotes (all arguments are joined)
python ralph/coding_agent_ralph_example.py Read the README file and summarize it
```

**Use cases:**
- One-off questions from scripts
- Quick queries without starting interactive mode
- Integration with other tools

### 3. Stdin Mode (Piping)

Pipe input to the script for batch processing or integration:

```bash
# From a file
cat prompt.md | python ralph/coding_agent_ralph_example.py

# From echo
echo "What is the project structure?" | python ralph/coding_agent_ralph_example.py

# From another command
generate_prompt.sh | python ralph/coding_agent_ralph_example.py
```

**Use cases:**
- Integration with `ralph.sh` (see below)
- Processing prompts from files
- Shell pipeline integration

## Running `ralph.sh`

The `ralph.sh` script runs the coding agent in a loop, feeding it the `prompt.md` file on each iteration until all tasks are complete.

### Basic Usage

```bash
# From the python directory
./ralph/ralph.sh

# Or with a custom max iterations (default is 10)
./ralph/ralph.sh 20
```

### How It Works

1. **Initialization**: 
   - Reads `prd.json` to get the current branch and PRD items
   - Creates/updates `progress.txt` to track progress
   - Archives previous runs if the branch changed

2. **Iteration Loop**:
   - For each iteration (up to `max_iterations`):
     - Reads `prompt.md` and pipes it to `coding_agent_ralph_example.py`
     - Captures the output
     - Checks for completion signal: `<promise>COMPLETE</promise>`
     - If complete, exits successfully
     - Otherwise, continues to next iteration

3. **Completion**:
   - Exits with code 0 if tasks complete
   - Exits with code 1 if max iterations reached
   - Exits with code 130 if interrupted (Ctrl+C)

### Key Files

- **`prd.json`**: Product Requirements Document with tasks to complete
- **`prompt.md`**: Instructions given to each agent iteration
- **`progress.txt`**: Progress log tracking what's been done
- **`archive/`**: Directory where previous runs are archived when branch changes
- **`.last-branch`**: Tracks the last branch name for archiving logic

### Example Output

```
Starting Ralph - Max iterations: 10

═══════════════════════════════════════════════════════
  Ralph Iteration 1 of 10
═══════════════════════════════════════════════════════
[Agent output...]
Iteration 1 complete. Continuing...

═══════════════════════════════════════════════════════
  Ralph Iteration 2 of 10
═══════════════════════════════════════════════════════
[Agent output...]
Iteration 2 complete. Continuing...

...

Ralph completed all tasks!
Completed at iteration 5 of 10
```

### Interrupting Ralph

Press **Ctrl+C** to gracefully interrupt Ralph. The script will:
- Clean up and exit with code 130
- Preserve progress made so far
- Allow you to resume later

## Environment Setup

Make sure you're in the `python` directory when running these scripts, as they depend on relative imports:

```bash
cd python
./ralph/ralph.sh
```

Or use absolute paths:

```bash
python /path/to/python/ralph/coding_agent_ralph_example.py "your question"
```

## Tips

- **For development**: Use interactive mode to test and debug
- **For automation**: Use command line args or stdin mode
- **For long-running tasks**: Use `ralph.sh` with appropriate max iterations
- **Monitor progress**: Check `progress.txt` to see what Ralph has accomplished
- **Review archives**: Check the `archive/` directory for previous run results

## Related Files

- `AGENTS.md`: Detailed documentation about Ralph's architecture and patterns
- `prompt.md`: The prompt template used by each iteration
- `prd.json`: Product requirements and tasks to complete
