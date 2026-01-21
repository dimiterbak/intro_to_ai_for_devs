const assert = require('assert');
const { isDraw, checkWinner } = require('./tic_tac_toe');

function boardFromPositions(positions) {
  const b = Array(9).fill(null);
  for (const [idx, val] of Object.entries(positions)) {
    b[Number(idx)] = val;
  }
  return b;
}

// Test 1: empty board -> not a draw
(function emptyBoard() {
  const b = Array(9).fill(null);
  assert.strictEqual(isDraw(b), false, 'Empty board should not be a draw');
})();

// Test 2: full board with no winner -> draw
(function fullNoWinner() {
  // A known draw position
  const b = ['X','O','X',
             'X','O','O',
             'O','X','X'];
  // sanity check: no winner
  const win = checkWinner(b);
  assert.strictEqual(win.winner, null, 'Sanity: this full board should have no winner');
  assert.strictEqual(isDraw(b), true, 'Full board with no winner should be a draw');
})();

// Test 3: full board but with a winner -> not a draw
(function fullWithWinner() {
  const b = ['X','X','X',
             'O','O',null,
             null,null,null];
  // board isn't full but has a winner; isDraw should be false
  assert.strictEqual(checkWinner(b).winner, 'X', 'Sanity: should detect X as winner');
  assert.strictEqual(isDraw(b), false, 'Board with a winner should not be a draw');
})();

console.log('All isDraw tests passed');
