const assert = require('assert');
const { checkWinner, isBoardFull } = require('./tic_tac_toe');

function boardFromPositions(positions) {
  const b = Array(9).fill(null);
  for (const [idx, val] of Object.entries(positions)) {
    b[Number(idx)] = val;
  }
  return b;
}

// Test 1: empty board -> no winner
(function emptyBoard() {
  const b = Array(9).fill(null);
  const res = checkWinner(b);
  assert.strictEqual(res.winner, null, 'Empty board should have no winner');
  assert.strictEqual(res.winningLine, null, 'Empty board should have no winning line');
})();

// Test 2: row win for X
(function rowWin() {
  const b = boardFromPositions({0: 'X', 1: 'X', 2: 'X'});
  const res = checkWinner(b);
  assert.strictEqual(res.winner, 'X', 'Top row should be a win for X');
  assert.deepStrictEqual(res.winningLine, [0,1,2]);
})();

// Test 3: column win for O
(function columnWin() {
  const b = boardFromPositions({2: 'O', 5: 'O', 8: 'O'});
  const res = checkWinner(b);
  assert.strictEqual(res.winner, 'O', 'Right column should be a win for O');
  assert.deepStrictEqual(res.winningLine, [2,5,8]);
})();

// Test 4: diagonal win for X (0,4,8)
(function diag1() {
  const b = boardFromPositions({0: 'X', 4: 'X', 8: 'X'});
  const res = checkWinner(b);
  assert.strictEqual(res.winner, 'X', 'Diagonal 0,4,8 should be a win for X');
  assert.deepStrictEqual(res.winningLine, [0,4,8]);
})();

// Test 5: diagonal win for O (2,4,6)
(function diag2() {
  const b = boardFromPositions({2: 'O', 4: 'O', 6: 'O'});
  const res = checkWinner(b);
  assert.strictEqual(res.winner, 'O', 'Diagonal 2,4,6 should be a win for O');
  assert.deepStrictEqual(res.winningLine, [2,4,6]);
})();

// Test 6: three marks not aligned -> no win
(function nonAlignedThree() {
  const b = boardFromPositions({0: 'X', 2: 'X', 4: 'X'});
  const res = checkWinner(b);
  assert.strictEqual(res.winner, null, 'Three Xs not aligned should not be a win');
})();

// Test 7: isBoardFull true/false
(function boardFullTests() {
  const full = Array(9).fill('X');
  assert.strictEqual(isBoardFull(full), true, 'Board filled with X should be full');
  const notFull = boardFromPositions({0: 'X', 1: 'O'});
  assert.strictEqual(isBoardFull(notFull), false, 'Partially filled board should not be full');
})();

console.log('All checkWinner and isBoardFull tests passed');
