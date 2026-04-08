import { Chess } from './chess.js'

let board = null
const game = new Chess()
const whiteSquareGrey = '#c9bea8'
const blackSquareGrey = '#9f8a6f'
const $status = window.jQuery('#status')
let engineModule = null
let aiBusy = false

async function initEngine() {
  if (typeof window.ChessEngineModule !== 'function') {
    throw new Error('Chess engine loader is not available')
  }

  engineModule = await window.ChessEngineModule()
}

function isGameOver() {
  return typeof game.isGameOver === 'function' ? game.isGameOver() : game.game_over()
}

function isCheckmate() {
  return typeof game.isCheckmate === 'function' ? game.isCheckmate() : game.in_checkmate()
}

function isDraw() {
  return typeof game.isDraw === 'function' ? game.isDraw() : game.in_draw()
}

function isCheck() {
  return typeof game.isCheck === 'function' ? game.isCheck() : game.in_check()
}

function onDragStart(source, piece) {
  if (aiBusy) return false
  if (isGameOver()) return false

  if (game.turn() !== 'w') return false

  if ((game.turn() === 'w' && /^b/.test(piece)) ||
      (game.turn() === 'b' && /^w/.test(piece))) {
    return false
  }
}

function onDrop(source, target) {
  removeGreySquares()

  let move = null

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

  updateStatus()
  queueAiMove()
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

function onSnapEnd() {
  board.position(game.fen())
}

function getBestMoveFromEngine(fen) {
  return engineModule.ccall('get_best_move', 'string', ['string'], [fen])
}

function toChessJsMove(moveText) {
  if (!moveText || moveText.length < 4) return null

  return {
    from: moveText.slice(0, 2),
    to: moveText.slice(2, 4),
    promotion: moveText.length >= 5 ? moveText[4] : 'q'
  }
}

function queueAiMove() {
  if (isGameOver()) return
  if (game.turn() !== 'b') return
  if (!engineModule) return

  aiBusy = true
  updateStatus()

  window.setTimeout(() => {
    const fen = game.fen()
    const engineMove = getBestMoveFromEngine(fen)
    const parsedMove = toChessJsMove(engineMove)

    if (parsedMove) {
      try {
        game.move(parsedMove)
      } catch (error) {
        const fallback = game.moves({ verbose: true })[0]
        if (fallback) game.move(fallback)
      }
    }

    board.position(game.fen())
    aiBusy = false
    updateStatus()
  }, 120)
}

function updateStatus() {
  let status = ''
  let moveColor = 'White'

  if (game.turn() === 'b') {
    moveColor = 'Black'
  }

  if (isCheckmate()) {
    status = 'Game over, ' + moveColor + ' is in checkmate.'
  } else if (isDraw()) {
    status = 'Game over, drawn position.'
  } else {
    status = moveColor + ' to move'

    if (aiBusy && moveColor === 'Black') {
      status += ' (AI thinking...)'
    }

    if (isCheck()) {
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
    onMouseoverSquare,
    onSnapEnd
  })

  updateStatus()
  window.addEventListener('resize', () => board.resize())
}

start().catch((error) => {
  $status.text('Engine failed to load: ' + error.message)
})
