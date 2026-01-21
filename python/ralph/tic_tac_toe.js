#!/usr/bin/env node
// tic_tac_toe.js
// Implementation for US-001 and US-002: Display empty board with numeric mapping
// and show welcome message with initialized session score
// Implementation for US-003: Accept player move input (1-9) from terminal
// Implementation for US-004: Validate player move input (reject non-numeric, out of range,
// occupied cells, empty input; re-prompt)
// Implementation for US-005: Apply player move and update board (applyPlayerMove)
// Implementation for US-006: Win detection (checkWinner) and isBoardFull
// Implementation for US-007: Draw detection (isDraw)
// Implementation for US-008: Optimal computer move logic (computeBestMove, minimax)

const readline = require('readline');

function createEmptyBoard() {
  return Array(9).fill(null);
}

function cellLabel(value, index, showMapping) {
  if (value === 'X' || value === 'O') return ` ${value} `;
  if (showMapping) return ` ${index + 1} `;
  return '   ';
}

function renderBoard(board, options = {}) {
  const showMapping = options.showMapping ?? true;
  const lineSep = '+-----+-----+-----+';
  const rows = [];
  for (let r = 0; r < 3; r++) {
    const i = r * 3;
    const a = cellLabel(board[i], i, showMapping);
    const b = cellLabel(board[i + 1], i + 1, showMapping);
    const c = cellLabel(board[i + 2], i + 2, showMapping);

    rows.push(lineSep);
    // make cells a bit larger for clarity
    rows.push(`| ${a} | ${b} | ${c} |`);
    rows.push(lineSep);
  }

  console.log('\nTic-Tac-Toe Board:');
  console.log(rows.join('\n'));
  console.log('');
}

function showWelcome(scores) {
  console.log('Welcome to Tic-Tac-Toe!');
  console.log('You are X and always move first.');
  console.log('Enter a number 1-9 to place your X in the corresponding cell:');
  console.log('Mapping: 1=top-left, 2=top-middle, 3=top-right, 4=middle-left, 5=center, 6=middle-right, 7=bottom-left, 8=bottom-middle, 9=bottom-right');
  console.log('Type q or quit at any prompt to exit.');
  console.log('');
  showScore(scores);
}

function showScore(scores) {
  // Ensure scores object has the counters
  const playerWins = scores?.playerWins ?? 0;
  const computerWins = scores?.computerWins ?? 0;
  const draws = scores?.draws ?? 0;
  console.log(`Session Score: Player Wins: ${playerWins}, Computer Wins: ${computerWins}, Draws: ${draws}`);
  console.log('');
}

// Prompt input helper using Node's readline, returns trimmed string
function promptInput(promptText) {
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
  return new Promise((resolve) => {
    rl.question(promptText, (answer) => {
      rl.close();
      resolve(String(answer).trim());
    });
  });
}

// US-003: prompt player for move (1-9). This function will re-prompt until a valid integer 1-9 is entered.
// It does NOT validate whether the cell is occupied (that belongs to US-004 / US-005).
async function promptForMove() {
  while (true) {
    const input = await promptInput('Enter your move (1-9): ');
    if (!input) {
      // empty input: just re-prompt
      console.log('Please enter a number between 1 and 9.');
      continue;
    }
    const n = Number(input);
    if (Number.isInteger(n) && n >= 1 && n <= 9) {
      return n; // return the 1-based cell number for now
    }
    // not a valid integer in range
    console.log('Invalid input. Please enter an integer between 1 and 9.');
  }
}

// US-004: Validate player move input against board occupancy and more robust checks.
// - Reject non-numeric input with friendly error
// - Reject numbers outside 1-9
// - Reject moves to occupied cells
// - Re-prompt after invalid input without crashing
// - Handle empty input and whitespace gracefully
async function promptForMoveValidated(board) {
  if (!Array.isArray(board) || board.length !== 9) {
    throw new Error('promptForMoveValidated requires a board array of length 9');
  }

  while (true) {
    const input = await promptInput('Enter your move (1-9): ');

    if (input === '') {
      console.log('No input detected. Please enter a number between 1 and 9 (e.g., 5 for center).');
      continue;
    }

    // Trim whitespace already done by promptInput; still guard
    const trimmed = String(input).trim();
    if (trimmed.length === 0) {
      console.log('Please enter a number between 1 and 9.');
      continue;
    }

    // Friendly handling for obvious non-numeric like letters
    const n = Number(trimmed);
    if (!Number.isFinite(n) || !Number.isInteger(n)) {
      console.log("That's not a number. Please type an integer between 1 and 9 (e.g., 3).");
      continue;
    }

    if (n < 1 || n > 9) {
      console.log('That number is out of range. Please enter a number from 1 to 9 corresponding to an empty cell.');
      continue;
    }

    const idx = n - 1;
    if (board[idx] === 'X' || board[idx] === 'O') {
      console.log(`Cell ${n} is already taken. Please choose a different empty cell.`);
      // Also show the board to help the player
      renderBoard(board, { showMapping: false });
      continue;
    }

    // All checks passed
    return n;
  }
}

function applyMove(board, cellNumber, mark) {
  if (!Array.isArray(board) || board.length !== 9) return false;
  const idx = cellNumber - 1;
  if (idx < 0 || idx > 8) return false;
  if (board[idx] === 'X' || board[idx] === 'O') return false;
  board[idx] = mark;
  return true;
}

// US-005: Apply player move wrapper - player always uses 'X'
function applyPlayerMove(board, cellNumber) {
  return applyMove(board, cellNumber, 'X');
}

// US-006: Implement win detection for rows, columns, diagonals
// checkWinner(board) -> { winner: 'X'|'O'|null, winningLine: [i,i,i]|null }
function checkWinner(board) {
  if (!Array.isArray(board) || board.length !== 9) {
    throw new Error('checkWinner requires a board array of length 9');
  }

  const lines = [
    [0, 1, 2],
    [3, 4, 5],
    [6, 7, 8],
    [0, 3, 6],
    [1, 4, 7],
    [2, 5, 8],
    [0, 4, 8],
    [2, 4, 6],
  ];

  for (const line of lines) {
    const [a, b, c] = line;
    const va = board[a];
    if (va && va === board[b] && va === board[c]) {
      return { winner: va, winningLine: [a, b, c] };
    }
  }

  return { winner: null, winningLine: null };
}

function isBoardFull(board) {
  if (!Array.isArray(board) || board.length !== 9) {
    throw new Error('isBoardFull requires a board array of length 9');
  }
  return board.every((cell) => cell === 'X' || cell === 'O');
}

// US-007: Draw detection
// A draw is when the board is full and there is no winner
function isDraw(board) {
  if (!Array.isArray(board) || board.length !== 9) {
    throw new Error('isDraw requires a board array of length 9');
  }
  const winnerRes = checkWinner(board);
  return winnerRes.winner === null && isBoardFull(board);
}

// US-008: Implement optimal computer move logic (Minimax)
// computeBestMove(board) -> number (0..8)
// minimax(board, isAiTurn) -> { score, move }
function computeBestMove(board) {
  if (!Array.isArray(board) || board.length !== 9) {
    throw new Error('computeBestMove requires a board array of length 9');
  }

  const availableMoves = [];
  for (let i = 0; i < 9; i++) {
    if (board[i] !== 'X' && board[i] !== 'O') availableMoves.push(i);
  }

  if (availableMoves.length === 0) return -1;

  let bestScore = -Infinity;
  let bestMove = availableMoves[0];

  for (const move of availableMoves) {
    // make move
    board[move] = 'O';
    const { score } = minimax(board, false);
    // undo
    board[move] = null;

    if (score > bestScore || (score === bestScore && move < bestMove)) {
      bestScore = score;
      bestMove = move;
    }
  }

  return bestMove;
}

function minimax(board, isAiTurn) {
  const winnerRes = checkWinner(board);
  if (winnerRes.winner === 'O') return { score: 1, move: null };
  if (winnerRes.winner === 'X') return { score: -1, move: null };
  if (isBoardFull(board)) return { score: 0, move: null };

  const availableMoves = [];
  for (let i = 0; i < 9; i++) {
    if (board[i] !== 'X' && board[i] !== 'O') availableMoves.push(i);
  }

  if (isAiTurn) {
    // AI (O) tries to maximize score
    let bestScore = -Infinity;
    let bestMove = availableMoves[0] ?? null;
    for (const move of availableMoves) {
      board[move] = 'O';
      const { score } = minimax(board, false);
      board[move] = null;
      if (score > bestScore || (score === bestScore && (bestMove === null || move < bestMove))) {
        bestScore = score;
        bestMove = move;
      }
    }
    return { score: bestScore, move: bestMove };
  } else {
    // Player (X) tries to minimize AI score
    let bestScore = Infinity;
    let bestMove = availableMoves[0] ?? null;
    for (const move of availableMoves) {
      board[move] = 'X';
      const { score } = minimax(board, true);
      board[move] = null;
      if (score < bestScore || (score === bestScore && (bestMove === null || move < bestMove))) {
        bestScore = score;
        bestMove = move;
      }
    }
    return { score: bestScore, move: bestMove };
  }
}

// If run directly, display the welcome, the mapping board, prompt for a single validated move, then exit
if (require.main === module) {
  (async () => {
    try {
      const board = createEmptyBoard();
      const scores = { playerWins: 0, computerWins: 0, draws: 0 };
      showWelcome(scores);
      console.log('Board mapping (1-9):');
      renderBoard(board, { showMapping: true });

      const move = await promptForMoveValidated(board);
      console.log(`You entered move: ${move} (cell ${move}). Applying move to board...`);
      const applied = applyPlayerMove(board, move);
      if (!applied) {
        console.log('Unexpected error: failed to apply move.');
        process.exit(1);
      }

      console.log('Updated board:');
      renderBoard(board, { showMapping: false });

      // Demonstrate win/draw detection (for manual verification)
      const result = checkWinner(board);
      if (result.winner) {
        console.log(`Winner detected: ${result.winner} on line ${result.winningLine}`);
      } else if (isDraw(board)) {
        console.log('Game ended in a draw (board full, no winner).');
      } else if (isBoardFull(board)) {
        // fallback (should be equivalent to isDraw)
        console.log('Board is full: draw');
      } else {
        // let the AI pick a move and show it
        const aiMove = computeBestMove(board);
        if (aiMove >= 0) {
          console.log(`Computer chooses cell ${aiMove + 1}`);
          board[aiMove] = 'O';
          renderBoard(board, { showMapping: false });
        }
        console.log('No winner yet.');
      }

      process.exit(0);
    } catch (err) {
      console.error('An unexpected error occurred:', err);
      process.exit(1);
    }
  })();
}

module.exports = { createEmptyBoard, renderBoard, showWelcome, showScore, promptInput, promptForMove, promptForMoveValidated, applyMove, applyPlayerMove, checkWinner, isBoardFull, isDraw, computeBestMove, minimax };
