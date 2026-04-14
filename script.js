import { Chess } from './chess.js'

let board = null
const game = new Chess()
const whiteSquareGrey = '#c9bea8'
const blackSquareGrey = '#9f8a6f'
const $status = window.jQuery('#status')
let engineModule = null
let aiBusy = false
let aiTimerId = null

async function initEngine() {
  if (typeof window.ChessEngineModule !== 'function') {
    throw new Error('Chess engine loader is not available')
  }

  engineModule = await window.ChessEngineModule()
}

function renderBoard(animate) {
  if (!board) return
  board.position(game.fen(), animate)
}

function clearAiTimer() {
  if (aiTimerId === null) return

  window.clearTimeout(aiTimerId)
  aiTimerId = null
}

function parseUciMove(uciMove) {
  if (typeof uciMove !== 'string') return null

  const trimmedMove = uciMove.trim().toLowerCase()
  if (!/^[a-h][1-8][a-h][1-8][nbrq]?$/.test(trimmedMove)) return null

  return {
    from: trimmedMove.slice(0, 2),
    to: trimmedMove.slice(2, 4),
    promotion: trimmedMove[4] ?? 'q'
  }
}

function moveFromEngine() {
  const fen = game.fen()
  const rawMove = engineModule.ccall('get_best_move', 'string', ['string'], [fen])
  const parsedMove = parseUciMove(rawMove)

  if (parsedMove) {
    const appliedMove = game.move(parsedMove)
    if (appliedMove) return
  }

  // If engine output is invalid, keep the game moving with a legal fallback.
  const fallbackMove = game.moves({ verbose: true })[0]
  if (fallbackMove) game.move(fallbackMove)
}

function onDragStart(source, piece) {
  if (aiBusy || aiTimerId !== null) return false
  if (game.isGameOver()) return false

  if (game.turn() !== 'w') return false

  return /^w/.test(piece)
}

function onDrop(source, target) {
  removeGreySquares()

  if (aiBusy || game.turn() !== 'w') return 'snapback'

  let move
  try {
    move = game.move({
      from: source,
      to: target,
      promotion: 'q'
    })
  } catch (error) {
    return 'snapback'
  }

  if (move === null) return 'snapback'

  // Force a full board sync immediately so castle rook / promotions stay in lockstep.
  renderBoard(false)
  updateStatus()
  scheduleAiMove()
}

function removeGreySquares() {
  window.jQuery('#board .square-55d63').css('background', '')
}

function greySquare(square) {
  const $square = window.jQuery('#board .square-' + square)

  let background = whiteSquareGrey
  if ($square.hasClass('black-3c85d')) {
    background = blackSquareGrey
  }

  $square.css('background', background)
}

function onMouseoverSquare(square) {
  const moves = game.moves({
    square,
    verbose: true
  })

  if (moves.length === 0) return

  greySquare(square)
  for (let i = 0; i < moves.length; i += 1) {
    greySquare(moves[i].to)
  }
}

function onMouseoutSquare() {
  removeGreySquares()
}

function runAiMove() {
  aiTimerId = null

  if (!engineModule || game.isGameOver() || game.turn() !== 'b') {
    updateStatus()
    return
  }

  aiBusy = true
  updateStatus()

  try {
    moveFromEngine()
  } catch (error) {
    const fallbackMove = game.moves({ verbose: true })[0]
    if (fallbackMove) game.move(fallbackMove)
  }

  renderBoard(true)
  aiBusy = false
  updateStatus()
}

function scheduleAiMove() {
  if (aiBusy || aiTimerId !== null) return
  if (!engineModule) return
  if (game.isGameOver() || game.turn() !== 'b') return

  aiTimerId = window.setTimeout(runAiMove, 180)
}

function updateStatus() {
  let status = ''
  const moveColor = game.turn() === 'b' ? 'Black' : 'White'

  if (game.isCheckmate()) {
    status = 'Game over, ' + moveColor + ' is in checkmate.'
  } else if (game.isDraw()) {
    status = 'Game over, drawn position.'
  } else {
    status = moveColor + ' to move'

    if (aiBusy && moveColor === 'Black') {
      status += ' (AI thinking...)'
    }

    if (game.isCheck()) {
      status += ', ' + moveColor + ' is in check'
    }
  }

  $status.text(status)
}

async function start() {
  await initEngine()

  board = window.Chessboard('board', {
    draggable: true,
    position: 'start',
    pieceTheme: 'chesspieces/wikipedia/{piece}.png',
    onDragStart,
    onDrop,
    onMouseoutSquare,
    onMouseoverSquare
  })

  renderBoard(false)
  updateStatus()
  window.addEventListener('resize', () => board.resize())
}

start().catch((error) => {
  clearAiTimer()
  $status.text('Engine failed to load: ' + error.message)
})
