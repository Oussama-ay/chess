import { Chess } from './chess.js'

let board = null
const game = new Chess()
const startFen = game.fen()
const whiteSquareGrey = '#c9bea8'
const blackSquareGrey = '#9f8a6f'
const $status = window.jQuery('#status')
const moveListEl = document.getElementById('move-list')
const boardWrapEl = document.getElementById('board-wrap')
const arrowCanvas = document.getElementById('arrow-layer')
const btnStartEl = document.getElementById('btn-start')
const btnPrevEl = document.getElementById('btn-prev')
const btnNextEl = document.getElementById('btn-next')
const btnEndEl = document.getElementById('btn-end')
let engineWorker = null
let engineReady = false
let pendingMoveResolve = null
let aiBusy = false
let aiTimerId = null
let moveTimeline = []
let timelineFens = [startFen]
let viewPly = 0
const arrows = new Set()
let arrowStartSquare = null

// Player color: 'white' or 'black' (persisted in localStorage)
let playerColor = 'white'
try { playerColor = localStorage.getItem('playerColor') || 'white' } catch (e) {}
const playerTurn = playerColor === 'white' ? 'w' : 'b'
const engineTurn = playerTurn === 'w' ? 'b' : 'w'

function isReviewMode() {
  return viewPly !== moveTimeline.length
}

function getFenAtPly(ply) {
  return timelineFens[ply] || startFen
}

function getSquareFromPoint(clientX, clientY) {
  const el = document.elementFromPoint(clientX, clientY)
  const squareEl = el?.closest('#board .square-55d63')
  if (!squareEl) return null

  for (const className of squareEl.classList) {
    if (/^square-[a-h][1-8]$/.test(className)) {
      return className.slice('square-'.length)
    }
  }

  return null
}

function getSquareCenter(square) {
  if (!boardWrapEl) return null
  const squareEl = boardWrapEl.querySelector('#board .square-' + square)
  if (!squareEl) return null

  const boardRect = boardWrapEl.getBoundingClientRect()
  const squareRect = squareEl.getBoundingClientRect()

  return {
    x: squareRect.left - boardRect.left + squareRect.width / 2,
    y: squareRect.top - boardRect.top + squareRect.height / 2
  }
}

function drawSingleArrow(ctx, from, to) {
  const start = getSquareCenter(from)
  const end = getSquareCenter(to)
  if (!start || !end) return

  const dx = end.x - start.x
  const dy = end.y - start.y
  const len = Math.hypot(dx, dy)
  if (len < 6) return

  const ux = dx / len
  const uy = dy / len
  const head = 16
  const bodyEndX = end.x - ux * head
  const bodyEndY = end.y - uy * head

  ctx.strokeStyle = 'rgba(211, 154, 53, 0.88)'
  ctx.fillStyle = 'rgba(211, 154, 53, 0.88)'
  ctx.lineWidth = 7
  ctx.lineCap = 'round'
  ctx.lineJoin = 'round'

  ctx.beginPath()
  ctx.moveTo(start.x, start.y)
  ctx.lineTo(bodyEndX, bodyEndY)
  ctx.stroke()

  const px = -uy
  const py = ux
  const wing = 7

  ctx.beginPath()
  ctx.moveTo(end.x, end.y)
  ctx.lineTo(bodyEndX + px * wing, bodyEndY + py * wing)
  ctx.lineTo(bodyEndX - px * wing, bodyEndY - py * wing)
  ctx.closePath()
  ctx.fill()
}

function resizeArrowCanvas() {
  if (!boardWrapEl || !arrowCanvas) return

  const rect = boardWrapEl.getBoundingClientRect()
  const dpr = window.devicePixelRatio || 1

  arrowCanvas.style.width = rect.width + 'px'
  arrowCanvas.style.height = rect.height + 'px'
  arrowCanvas.width = Math.max(1, Math.round(rect.width * dpr))
  arrowCanvas.height = Math.max(1, Math.round(rect.height * dpr))

  const ctx = arrowCanvas.getContext('2d')
  if (!ctx) return
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
}

function redrawArrows() {
  if (!boardWrapEl || !arrowCanvas) return
  const ctx = arrowCanvas.getContext('2d')
  if (!ctx) return

  const rect = boardWrapEl.getBoundingClientRect()
  ctx.clearRect(0, 0, rect.width, rect.height)

  for (const arrow of arrows) {
    const [from, to] = arrow.split('->')
    if (from && to) drawSingleArrow(ctx, from, to)
  }
}

function setupArrowEvents() {
  if (!boardWrapEl) return

  const onContextMenu = (event) => {
    const square = getSquareFromPoint(event.clientX, event.clientY)
    if (!square) return
    event.preventDefault()
    event.stopPropagation()
  }

  const onMouseDown = (event) => {
    if (event.button !== 2) return
    const square = getSquareFromPoint(event.clientX, event.clientY)
    if (!square) return

    event.preventDefault()
    event.stopPropagation()
    arrowStartSquare = square
  }

  const onMouseUp = (event) => {
    if (event.button !== 2 || !arrowStartSquare) return
    const square = getSquareFromPoint(event.clientX, event.clientY)
    if (!square || square === arrowStartSquare) {
      arrowStartSquare = null
      return
    }

    event.preventDefault()
    event.stopPropagation()

    const key = arrowStartSquare + '->' + square
    if (arrows.has(key)) {
      arrows.delete(key)
    } else {
      arrows.add(key)
    }

    arrowStartSquare = null
    redrawArrows()
  }

  // Use capture phase so this still works when chessboard has draggable pieces
  // and stops/binds mouse events during the player's turn.
  boardWrapEl.addEventListener('contextmenu', onContextMenu, true)
  boardWrapEl.addEventListener('mousedown', onMouseDown, true)
  boardWrapEl.addEventListener('mouseup', onMouseUp, true)

  window.addEventListener('mouseup', () => {
    arrowStartSquare = null
  }, true)
}

function updateTimeline(jumpToLatest = true) {
  const verbose = game.history({ verbose: true })
  moveTimeline = verbose.map((move) => ({
    san: move.san,
    from: move.from,
    to: move.to,
    promotion: move.promotion ?? 'q'
  }))

  const replay = new Chess(startFen)
  timelineFens = [startFen]

  for (const move of moveTimeline) {
    replay.move({ from: move.from, to: move.to, promotion: move.promotion })
    timelineFens.push(replay.fen())
  }

  if (jumpToLatest || viewPly > moveTimeline.length) {
    viewPly = moveTimeline.length
  }
}

function renderMoveList() {
  if (!moveListEl) return

  moveListEl.innerHTML = ''

  if (moveTimeline.length === 0) {
    const empty = document.createElement('div')
    empty.className = 'move-row'
    empty.textContent = 'No moves yet.'
    moveListEl.appendChild(empty)
    return
  }

  for (let i = 0; i < moveTimeline.length; i += 2) {
    const row = document.createElement('div')
    row.className = 'move-row'

    const no = document.createElement('span')
    no.className = 'move-no'
    no.textContent = String(i / 2 + 1) + '.'
    row.appendChild(no)

    const whiteBtn = document.createElement('button')
    whiteBtn.type = 'button'
    whiteBtn.className = 'move-btn'
    whiteBtn.dataset.ply = String(i + 1)
    whiteBtn.textContent = moveTimeline[i].san
    if (viewPly === i + 1) whiteBtn.classList.add('active')
    row.appendChild(whiteBtn)

    const blackBtn = document.createElement('button')
    blackBtn.type = 'button'
    blackBtn.className = 'move-btn'
    if (moveTimeline[i + 1]) {
      blackBtn.dataset.ply = String(i + 2)
      blackBtn.textContent = moveTimeline[i + 1].san
      if (viewPly === i + 2) blackBtn.classList.add('active')
    } else {
      blackBtn.disabled = true
      blackBtn.textContent = '—'
    }
    row.appendChild(blackBtn)

    moveListEl.appendChild(row)
  }
}

function updateMoveControls() {
  if (!btnStartEl || !btnPrevEl || !btnNextEl || !btnEndEl) return

  btnStartEl.disabled = viewPly === 0
  btnPrevEl.disabled = viewPly === 0
  btnNextEl.disabled = viewPly === moveTimeline.length
  btnEndEl.disabled = viewPly === moveTimeline.length
}

function syncBoardToView(animate = false) {
  if (!board) return
  board.position(getFenAtPly(viewPly), animate)
  redrawArrows()
}

function goToPly(ply) {
  viewPly = Math.max(0, Math.min(moveTimeline.length, ply))
  syncBoardToView(false)
  renderMoveList()
  updateMoveControls()
  updateStatus()

  if (!isReviewMode()) {
    scheduleAiMove()
  }
}

function setupMoveControls() {
  if (moveListEl) {
    moveListEl.addEventListener('click', (event) => {
      const btn = event.target.closest('button[data-ply]')
      if (!btn) return
      clearAiTimer()
      goToPly(Number(btn.dataset.ply))
    })
  }

  btnStartEl?.addEventListener('click', () => {
    clearAiTimer()
    goToPly(0)
  })

  btnPrevEl?.addEventListener('click', () => {
    clearAiTimer()
    goToPly(viewPly - 1)
  })

  btnNextEl?.addEventListener('click', () => {
    goToPly(viewPly + 1)
  })

  btnEndEl?.addEventListener('click', () => {
    goToPly(moveTimeline.length)
  })
}

function initEngine() {
  return new Promise((resolve, reject) => {
    try {
      engineWorker = new Worker('./engine-worker.js')
    } catch (e) {
      reject(new Error('Failed to create Web Worker: ' + e.message))
      return
    }

    engineWorker.onmessage = (e) => {
      const data = e.data
      if (data.type === 'ready') {
        engineReady = true
        resolve()
      } else if (data.type === 'move') {
        if (pendingMoveResolve) {
          const res = pendingMoveResolve
          pendingMoveResolve = null
          res(data.move)
        }
      } else if (data.type === 'error') {
        console.error('Engine worker error:', data.error)
        if (pendingMoveResolve) {
          const res = pendingMoveResolve
          pendingMoveResolve = null
          res(null)
        }
      }
    }

    engineWorker.onerror = (err) => {
      reject(new Error('Web Worker error: ' + err.message))
    }
  })
}

function renderBoard(animate) {
  if (!board) return
  board.position(getFenAtPly(viewPly), animate)
  redrawArrows()
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

function moveFromEngineAsync() {
  return new Promise((resolve) => {
    pendingMoveResolve = resolve
    engineWorker.postMessage({ fen: game.fen() })
  })
}

function onDragStart(source, piece) {
  if (aiBusy || aiTimerId !== null) return false
  if (game.isGameOver()) return false
  if (isReviewMode()) return false

  if (game.turn() !== playerTurn) return false

  if (playerTurn === 'w') return /^w/.test(piece)
  return /^b/.test(piece)
}

function onDrop(source, target) {
  removeGreySquares()

  if (isReviewMode()) return 'snapback'
  if (aiBusy || game.turn() !== playerTurn) return 'snapback'

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

  updateTimeline(true)

  // Force a full board sync immediately so castle rook / promotions stay in lockstep.
  renderBoard(false)
  renderMoveList()
  updateMoveControls()
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

async function runAiMove() {
  aiTimerId = null

  if (isReviewMode()) {
    updateStatus()
    return
  }

  if (!engineReady || game.isGameOver() || game.turn() !== engineTurn) {
    updateStatus()
    return
  }

  const expectedFen = game.fen()
  aiBusy = true
  updateStatus()

  try {
    const rawMove = await moveFromEngineAsync()
    
    // Discard result if game state changed while searching
    if (isReviewMode() || game.fen() !== expectedFen || game.turn() !== engineTurn) {
      aiBusy = false
      updateStatus()
      return
    }

    const parsedMove = parseUciMove(rawMove)
    if (parsedMove && game.move(parsedMove)) {
      // ok
    } else {
      const fallbackMove = game.moves({ verbose: true })[0]
      if (fallbackMove) game.move(fallbackMove)
    }
  } catch (error) {
    if (isReviewMode() || game.fen() !== expectedFen || game.turn() !== engineTurn) {
      aiBusy = false
      updateStatus()
      return
    }
    const fallbackMove = game.moves({ verbose: true })[0]
    if (fallbackMove) game.move(fallbackMove)
  }

  updateTimeline(true)

  renderBoard(true)
  renderMoveList()
  updateMoveControls()
  aiBusy = false
  updateStatus()
}

function scheduleAiMove() {
  if (aiBusy || aiTimerId !== null) return
  if (!engineReady) return
  if (isReviewMode()) return
  if (game.isGameOver() || game.turn() !== engineTurn) return

  aiTimerId = window.setTimeout(runAiMove, 180)
}

function updateStatus() {
  const viewGame = new Chess(getFenAtPly(viewPly))
  let status = ''
  const moveColor = viewGame.turn() === 'b' ? 'Black' : 'White'

  if (viewGame.isCheckmate()) {
    status = 'Game over, ' + moveColor + ' is in checkmate.'
  } else if (viewGame.isDraw()) {
    status = 'Game over, drawn position.'
  } else {
    status = moveColor + ' to move'

    if (!isReviewMode() && aiBusy && moveColor === (engineTurn === 'b' ? 'Black' : 'White')) {
      status += ' (AI thinking...)'
    }

    if (viewGame.isCheck()) {
      status += ', ' + moveColor + ' is in check'
    }
  }

  if (isReviewMode()) {
    status = 'Reviewing move ' + viewPly + '/' + moveTimeline.length + ' — ' + status
  }

  $status.text(status)
}

async function start() {
  await initEngine()

  board = window.Chessboard('board', {
    draggable: true,
    position: 'start',
    orientation: playerColor === 'white' ? 'white' : 'black',
    pieceTheme: 'chesspieces/wikipedia/{piece}.png',
    onDragStart,
    onDrop,
    onMouseoutSquare,
    onMouseoverSquare
  })

  setupMoveControls()
  setupArrowEvents()
  resizeArrowCanvas()

  updateTimeline(true)

  // If the engine moves first (player chose black), schedule AI immediately.
  if (game.turn() === engineTurn) scheduleAiMove()

  renderBoard(false)
  renderMoveList()
  updateMoveControls()
  updateStatus()
  window.addEventListener('resize', () => {
    board.resize()
    resizeArrowCanvas()
    redrawArrows()
  })
}

start().catch((error) => {
  clearAiTimer()
  $status.text('Engine failed to load: ' + error.message)
})
