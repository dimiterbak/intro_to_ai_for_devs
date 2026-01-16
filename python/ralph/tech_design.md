# Technical Design Document — Tic‑Tac‑Toe (Node.js CLI)

## Project Overview
A simple, dependency‑free Node.js command‑line Tic‑Tac‑Toe game where a human (X) always goes first and plays against an unbeatable computer (O). The implementation will be a single CommonJS JavaScript file runnable with node. The AI will use the Minimax algorithm (deterministic) and tie‑break using the lowest numbered cell to keep behavior predictable.

## Goals and Constraints
- Platform: Node.js (latest stable). CommonJS modules (require/module.exports) will be used.
- Single file implementation for easy execution and distribution (e.g., tic_tac_toe.js).
- No external npm dependencies.
- CLI input/output using Node's built‑in APIs (readline, process).
- Unbeatable AI (optimal play) using Minimax.
- Deterministic tie breaking: when multiple optimal moves exist, pick the lowest numbered cell (1–9).
- In‑session scoring: track Player wins, Computer wins, Draws (kept in memory; not persisted to disk).
- Exit options: accept `q` or `quit` at prompts; handle Ctrl+C gracefully.

## Files and Layout
Single file:
- tic_tac_toe.js — contains entire application (game logic, AI, CLI UI, scoring, main loop).

Other artifacts (not required but recommended in repository):
- README.md — run instructions (e.g., `node tic_tac_toe.js`).
- PRD and this Technical Design document (already provided).

## High‑level Components
All implemented inside the single file as logically separated functions:
- CLI / UI
  - renderBoard(board, showMapping=false)
  - promptInput(promptText)
  - showMessage(text)
  - clearScreen() [optional]
- Game state & rules
  - createEmptyBoard() -> board
  - isValidMove(board, moveIndex)
  - applyMove(board, moveIndex, mark)
  - checkWinner(board) -> { winner: 'X' | 'O' | null, winningLine: [i,i,i] | null }
  - isBoardFull(board)
- Game loop
  - playRound() — run a single round until win/draw
  - main() — session loop: show welcome, scores, repeat rounds until quit
- AI (Minimax)
  - computeBestMove(board) -> moveIndex
  - minimax(board, isAiTurn) -> { score, moveIndex }

## Data Model
- Board: a length‑9 array indexed 0..8 representing cells 1..9 (mapping: index = cellNumber - 1).
  - Values: 'X' for player, 'O' for computer, null (or ' ') for empty.
- Player marks: playerMark = 'X', aiMark = 'O'.
- Scores (in memory): { playerWins: number, aiWins: number, draws: number }

Cell mapping (for user):
1 2 3
4 5 6
7 8 9

Internally index mapping: index = inputNumber - 1.

## User Interaction and Input Handling
- Use Node's readline module to prompt the player.
- Prompt text explains numeric mapping and accepts input of:
  - A number 1–9 corresponding to an empty cell.
  - `q` or `quit` to exit the game.
- Input validation:
  - Non‑numeric or out‑of‑range inputs -> friendly error + re‑prompt.
  - Numeric input corresponding to an occupied cell -> friendly error + re‑prompt.
- After the player move, update board and print it. Then compute and apply AI move, print board.
- After game end, announce result with child‑friendly message and update the session scores. Then ask "Play again? (y/n)".

Implementation notes:
- Use a small helper to normalize and trim input; compare lowercase for `q`/`quit`.
- For simplicity, prompt / re‑prompt loops will be implemented via async/await inside an async main() function called at the bottom of the file.
  - Even though CommonJS is used, an async function can be defined and invoked (e.g., `async function main(){...}; main().catch(err=>...)`).

## Win Detection
Implement checkWinner(board) that checks the eight winning combinations (3 rows, 3 columns, 2 diagonals). Return:
- { winner: 'X' | 'O' | null, winningLine: [i,i,i] | null }

A draw is defined as board full and winner === null.

## AI Design (Minimax)
- Purpose: compute the optimal move for the AI (O) that never allows a loss when both sides play optimally.
- Minimax specifics:
  - Terminal utilities (from AI perspective):
    - If AI wins -> score = +1
    - If Player wins -> score = -1
    - If draw -> score = 0
  - The minimax function recursively explores all legal moves, alternates players, and returns the best attainable score and the corresponding move.
  - To keep behavior deterministic when multiple moves yield the same score, tie‑break by selecting the move with the lowest board index (which corresponds to lowest user cell number 1–9).
  - Alpha‑beta pruning is optional. Given Tic‑Tac‑Toe's small game tree, a simple minimax without pruning is acceptable and clearer; however adding alpha‑beta is straightforward and will reduce recursion.
- Implementation details:
  - computeBestMove(board) will iterate available moves and call minimax for each.
  - minimax(board, isAiTurn) will:
    - Check for terminal state via checkWinner or board full.
    - For each available move:
      - Simulate move (mutate a copy or undo after recursion).
      - Recurse with toggled isAiTurn.
      - Collect best score and track best move.
    - Return { score, moveIndex }.
  - Use small optimizations: pass board as an array and perform in‑place change + undo to reduce allocations.

Complexity:
- Worst case ~9! nodes visited; practically tiny and instantaneous on modern hardware.

## Determinism / Tie‑breaking
- When minimax finds multiple moves with equal scores, choose the one with the smallest index — this ensures reproducible behavior and matches the user's request for predictable choices.

## Error Handling and Robustness
- Guard against unexpected exceptions and report a friendly error message, but keep the app running or exit cleanly.
- Gracefully handle Ctrl+C (SIGINT) by closing readline and printing a goodbye message.

## Example Function Signatures (in single file)
- function renderBoard(board, showMapping=false)
- function createEmptyBoard() -> Array(9)
- function checkWinner(board) -> { winner, winningLine }
- function isBoardFull(board) -> boolean
- function computeBestMove(board) -> number (0..8)
- function minimax(board, isAiTurn) -> { score, move }
- function promptInput(promptText) -> Promise<string>
- async function playRound() -> result ('player'|'ai'|'draw')
- async function main() -> void

## CLI / UX Details
- Welcome message: short and friendly, show current session score and brief instructions.
- Before first round, show numeric mapping (renderBoard with numbers) so player sees the mapping.
- After each move, display an ASCII board with X and O populated.
- End messages tailored for child audience, e.g.:
  - Player win: "Great job! You got three in a row!"
  - AI win: "The computer won this time — try a different move next round!"
  - Draw: "Nice — it's a draw! Both players were careful."
- Replay prompt: "Play again? (y/n)"; accept `y`/`yes` to continue, anything else to quit.

## Acceptance Criteria (technical)
- The program runs via `node tic_tac_toe.js` and opens a playable CLI.
- Player input 1..9 places an X in the correct cell when empty.
- Invalid inputs and occupied cells are rejected with friendly messages and re‑prompts.
- AI never loses (verify by manual play or spot checks); it either wins or forces a draw when possible.
- Score counters increment correctly for player wins, AI wins, and draws and are displayed between rounds.
- The app accepts `q` or `quit` at move prompts and exits gracefully.

## Implementation Notes and Gotchas
- Keep internal board values consistent (use null for empty or a single sentinel like ' '). Prefer null to differentiate empty from string marks.
- When mutating the board for minimax, ensure moves are undone after recursion to avoid corrupting the state used by other branches.
- Because the player always goes first (X) and AI is optimal (O), many games will end in draws — messaging should encourage learning rather than frustration.

## Run Instructions (to include in README)
- Run: node tic_tac_toe.js
- Controls: Type 1–9 to place X in that cell; type `q` or `quit` to exit; after each round choose to replay or quit.

## Future Enhancements (out of scope for current design)
- Add optional difficulty levels (easy/medium/hard) by introducing randomness or limited search depth.
- Add unit tests (using a test framework or Node's assert) for checkWinner, minimax, and other core functions.
- Add persistent scoring (save session scores to a simple JSON file).
- Convert to ES Module structure and split into modules for larger codebase organization.

