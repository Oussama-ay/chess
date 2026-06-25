#include "chess/search.h"
#include "chess/attack.h"
#include "chess/board.h"
#include "chess/eval.h"
#include "chess/movegen.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

static constexpr int kInfinity = 100000000;
static constexpr int kMateScore = kInfinity - 1000;
static constexpr int kTimeCheckInterval = 2048;
static constexpr int kMaxQuiescencePly = 12;

struct SearchContext {
	bool stopped = false;
	int nodesSinceCheck = 0;
	std::chrono::steady_clock::time_point deadline;

	explicit SearchContext(int timeMs)
		: deadline(std::chrono::steady_clock::now() +
				   std::chrono::milliseconds(std::max(1, timeMs))) {}
};

static int piece_value(int pieceType) {
	switch (pieceType) {
		case PAWN:   return 100;
		case KNIGHT: return 320;
		case BISHOP: return 330;
		case ROOK:   return 500;
		case KING:   return 20000;
		case QUEEN:  return 900;
		default:	 return 0;
	}
}

static int move_score(const Board& board, const Move& move) {
	int score = 0;
	int captured = board.squares[move.toRow][move.toCol];
	if (move.isEnPassant) captured = board.whiteToMove ? -PAWN : PAWN;

	if (captured != 0) {
		const int moving = board.squares[move.fromRow][move.fromCol];
		score += 10000 + piece_value(abs_piece(captured)) * 16 -
				 piece_value(abs_piece(moving));
	}
	if (move.promotion != 0) score += 20000 + piece_value(move.promotion);
	if (move.isCastling) score += 50;

	return score;
}

static void order_moves(const Board& board, std::vector<Move>& moves) {
	std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
		return move_score(board, a) > move_score(board, b);
	});
}

static bool should_stop(SearchContext& ctx) {
	if (ctx.stopped) return true;

	if (++ctx.nodesSinceCheck < kTimeCheckInterval) return false;

	ctx.nodesSinceCheck = 0;
	if (std::chrono::steady_clock::now() >= ctx.deadline) {
		ctx.stopped = true;
	}

	return ctx.stopped;
}

static int side_to_move_score(const Board& board) {
	const int score = evaluate(board);
	return board.whiteToMove ? score : -score;
}

static bool is_capture_or_promotion(const Board& board, const Move& move) {
	return move.promotion != 0 || move.isEnPassant ||
		   board.squares[move.toRow][move.toCol] != 0;
}

static int quiesce(Board& board, int alpha, int beta,
				   std::vector<BoardState>& stateStack, int ply,
				   SearchContext& ctx,
				   std::vector<std::vector<Move>>& moveLists) {
	if (should_stop(ctx)) return 0;
	if (board.halfMoveClock >= 100) return 0;
	if (ply >= static_cast<int>(moveLists.size())) return side_to_move_score(board);

	const bool inCheck = is_in_check(board, board.whiteToMove);
	int best = -kInfinity;

	if (!inCheck) {
		best = side_to_move_score(board);
		if (best >= beta) return best;
		if (best > alpha) alpha = best;
	}

	std::vector<Move>& moves = moveLists[ply];
	generate_legal_moves(board, stateStack, moves);

	if (moves.empty()) {
		return inCheck ? (-kMateScore + ply) : 0;
	}

	order_moves(board, moves);

	for (const Move& move : moves) {
		if (!inCheck && !is_capture_or_promotion(board, move)) continue;

		make_move(board, move, stateStack);
		const int score = -quiesce(board, -beta, -alpha, stateStack,
								   ply + 1, ctx, moveLists);
		undo_move(board, move, stateStack);

		if (ctx.stopped) break;

		if (score > best) best = score;
		if (score > alpha) alpha = score;
		if (alpha >= beta) break;
	}

	return best;
}

static int negamax(Board& board, int depth, int alpha, int beta,
				   std::vector<BoardState>& stateStack, int ply,
				   SearchContext& ctx,
				   std::vector<std::vector<Move>>& moveLists) {
	if (should_stop(ctx)) return 0;
	if (board.halfMoveClock >= 100) return 0;
	if (depth == 0)
		return quiesce(board, alpha, beta, stateStack, ply, ctx, moveLists);

	std::vector<Move>& moves = moveLists[ply];
	generate_legal_moves(board, stateStack, moves);

	if (moves.empty()) {
		return is_in_check(board, board.whiteToMove) ? (-kMateScore + ply) : 0;
	}

	order_moves(board, moves);

	int best = -kInfinity;
	for (const Move& move : moves) {
		make_move(board, move, stateStack);
		const int score = -negamax(board, depth - 1, -beta, -alpha, stateStack,
								   ply + 1, ctx, moveLists);
		undo_move(board, move, stateStack);

		if (ctx.stopped) break;

		if (score > best) best = score;
		if (score > alpha) alpha = score;
		if (alpha >= beta) break;
	}

	return best;
}

static bool is_null_move(const Move& move) {
	return move.fromRow == 0 && move.fromCol == 0 &&
		   move.toRow == 0 && move.toCol == 0 &&
		   move.promotion == 0 && !move.isCastling && !move.isEnPassant;
}

static char promotion_char(int piece) {
	switch (piece) {
		case KNIGHT: return 'n';
		case BISHOP: return 'b';
		case ROOK:   return 'r';
		default:	 return 'q';
	}
}

} // namespace

void move_to_uci(const Move& move, char out[6]) {
	if (is_null_move(move)) {
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
	} else {
		out[4] = '\0';
	}
}

Move search_best_move(Board& board, int maxDepth, int timeMs,
					  std::vector<BoardState>& stateStack) {
	if (maxDepth < 1) maxDepth = 1;

	std::vector<std::vector<Move>> moveLists(maxDepth + kMaxQuiescencePly + 2);
	for (std::vector<Move>& list : moveLists) list.reserve(64);

	std::vector<Move>& moves = moveLists[0];
	generate_legal_moves(board, stateStack, moves);
	if (moves.empty()) return {0, 0, 0, 0, 0, false, false};

	Move bestMove = moves.front();
	SearchContext ctx(timeMs);

	for (int depth = 1; depth <= maxDepth; ++depth) {
		if (should_stop(ctx)) break;

		order_moves(board, moves);

		int alpha = -kInfinity;
		int best = -kInfinity;
		Move depthBest = moves.front();

		for (const Move& move : moves) {
			make_move(board, move, stateStack);
			const int score = -negamax(board, depth - 1, -kInfinity, -alpha,
									   stateStack, 1, ctx, moveLists);
			undo_move(board, move, stateStack);

			if (ctx.stopped) break;

			if (score > best) {
				best = score;
				depthBest = move;
			}
			if (score > alpha) alpha = score;
		}

		if (ctx.stopped) break;
		bestMove = depthBest;
	}

	return bestMove;
}
