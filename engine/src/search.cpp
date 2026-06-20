#include "chess/search.h"
#include "chess/attack.h"
#include "chess/board.h"
#include "chess/eval.h"
#include "chess/movegen.h"
#include "chess/tt.h"

#include <algorithm>
#include <vector>

namespace {

static int piece_value(int pieceType) {
  switch (pieceType) {
  case PAWN:
    return 100;
  case KNIGHT:
    return 320;
  case BISHOP:
    return 330;
  case ROOK:
    return 500;
  case KING:
    return 20000;
  case QUEEN:
    return 900;
  default:
    return 0;
  }
}

static int move_score(const Board &board, const Move &move) {
  int score = 0;
  int captured = board.squares[move.toRow][move.toCol];
  if (move.isEnPassant)
    captured = board.whiteToMove ? -PAWN : PAWN;

  if (captured != 0) {
    int movingValue =
        piece_value(abs_piece(board.squares[move.fromRow][move.fromCol]));
    score += 10000 + piece_value(abs_piece(captured)) * 16 - movingValue;
  }
  if (move.promotion != 0)
    score += 8000 + piece_value(move.promotion);
  if (move.isCastling)
    score += 50;
  return score;
}

static void order_moves(const Board &board, std::vector<Move> &moves,
                        const Move *preferred) {
  std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
    int sa = move_score(board, a), sb = move_score(board, b);
    if (preferred) {
      if (a == *preferred)
        sa += 1000000;
      if (b == *preferred)
        sb += 1000000;
    }
    return sa > sb;
  });
}

static constexpr int kMaxPly = 128;

static int quiesce(Board &board, int alpha, int beta,
                   std::vector<BoardState> &stateStack, int ply) {
  if (board.halfMoveClock >= 100)
    return 0;

  const bool inCheck = is_in_check(board, board.whiteToMove);

  // Safety guard against very long forcing/check sequences.
  if (ply >= kMaxPly) {
    int e = evaluate(board);
    return board.whiteToMove ? e : -e;
  }

  int best;

  // Stand-pat is only legal when the side to move is NOT in check.
  if (inCheck) {
    best = -kInfinity;
  } else {
    int e = evaluate(board);
    int standPat = board.whiteToMove ? e : -e;

    if (standPat >= beta)
      return standPat;

    if (standPat > alpha)
      alpha = standPat;

    best = standPat;
  }

  std::vector<Move> moves;
  moves.reserve(64);
  generate_legal_moves(board, stateStack, moves);

  if (moves.empty())
    return inCheck ? (-kMateScore + ply) : 0;

  order_moves(board, moves, nullptr);

  for (const Move &move : moves) {
    if (!inCheck) {
      bool isCapture =
          board.squares[move.toRow][move.toCol] != 0 || move.isEnPassant;

      // In quiet positions, quiescence only searches captures,
      // en passant, and promotions.
      if (!isCapture && move.promotion == 0)
        continue;
    }

    make_move(board, move, stateStack);
    int score = -quiesce(board, -beta, -alpha, stateStack, ply + 1);
    undo_move(board, move, stateStack);

    if (score > best)
      best = score;
    if (score > alpha)
      alpha = score;
    if (alpha >= beta)
      break;
  }
  return best;
}

static int negamax(Board &board, int depth, int alpha, int beta,
                   std::vector<BoardState> &stateStack, int ply) {
  if (board.halfMoveClock >= 100)
    return 0;

  int origAlpha = alpha, origBeta = beta;

  TTEntry *ttEntry = probe_tt(board.hash);
  Move ttMove{0, 0, 0, 0, 0, false, false};
  if (ttEntry) {
    ttMove = ttEntry->bestMove;
    if (ttEntry->depth >= depth) {
      int s = score_from_tt(ttEntry->score, ply);
      switch (ttEntry->flag) {
      case TTFlag::Exact:
        return s;
      case TTFlag::LowerBound:
        if (s >= beta)
          return s;
        alpha = std::max(alpha, s);
        break;
      case TTFlag::UpperBound:
        if (s <= alpha)
          return s;
        beta = std::min(beta, s);
        break;
      }
    }
  }

  if (depth == 0)
    return quiesce(board, alpha, beta, stateStack, ply);

  std::vector<Move> moves;
  moves.reserve(64);
  generate_legal_moves(board, stateStack, moves);

  if (moves.empty())
    return is_in_check(board, board.whiteToMove) ? (-kMateScore + ply) : 0;

  order_moves(board, moves, ttEntry ? &ttMove : nullptr);

  int best = -kInfinity;
  Move bestMove = moves.front();

  for (const Move &move : moves) {
    make_move(board, move, stateStack);
    int score = -negamax(board, depth - 1, -beta, -alpha, stateStack, ply + 1);
    undo_move(board, move, stateStack);

    if (score > best) {
      best = score;
      bestMove = move;
    }
    if (score > alpha)
      alpha = score;
    if (alpha >= beta)
      break;
  }

  TTFlag flag = TTFlag::Exact;
  if (best <= origAlpha)
    flag = TTFlag::UpperBound;
  else if (best >= origBeta)
    flag = TTFlag::LowerBound;

  store_tt(board.hash, depth, score_to_tt(best, ply), flag, bestMove);
  return best;
}

} // namespace

static char promotion_char(int p) {
  switch (p) {
  case KNIGHT:
    return 'n';
  case BISHOP:
    return 'b';
  case ROOK:
    return 'r';
  default:
    return 'q';
  }
}

void move_to_uci(const Move &move, char out[6]) {
  if (move == Move{0, 0, 0, 0, 0, false, false}) {
    out[0] = '0';
    out[1] = '0';
    out[2] = '0';
    out[3] = '0';
    out[4] = '\0';
    return;
  }
  out[0] = 'a' + move.fromCol;
  out[1] = '1' + move.fromRow;
  out[2] = 'a' + move.toCol;
  out[3] = '1' + move.toRow;
  if (move.promotion != 0) {
    out[4] = promotion_char(move.promotion);
    out[5] = '\0';
  } else
    out[4] = '\0';
}

Move search_best_move(Board &board, int depth,
                      std::vector<BoardState> &stateStack) {
  std::vector<Move> moves;
  moves.reserve(64);
  generate_legal_moves(board, stateStack, moves);
  if (moves.empty())
    return {0, 0, 0, 0, 0, false, false};

  Move bestMove = moves.front();
  if (TTEntry *e = probe_tt(board.hash))
    bestMove = e->bestMove;

  for (int d = 1; d <= depth; ++d) {
    order_moves(board, moves, &bestMove);

    int alpha = -kInfinity, best = -kInfinity;
    Move iterBest = moves.front();

    for (const Move &move : moves) {
      make_move(board, move, stateStack);
      int score = -negamax(board, d - 1, -kInfinity, -alpha, stateStack, 1);
      undo_move(board, move, stateStack);

      if (score > best) {
        best = score;
        iterBest = move;
      }
      if (score > alpha)
        alpha = score;
    }

    store_tt(board.hash, d, score_to_tt(best, 0), TTFlag::Exact, iterBest);
    bestMove = iterBest;
  }

  return bestMove;
}
