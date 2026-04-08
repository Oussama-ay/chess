#include "chess/search.h"

#include "chess/eval.h"
#include "chess/movegen.h"

static int minimax(Board& board, int depth, int alpha, int beta, bool maximizing, std::vector<BoardState>& stateStack)
{
    if (board.halfMoveClock >= 100) {
        return 0;
    }

    if (depth == 0) {
        return evaluate(board);
    }

    const std::vector<Move> legalMoves = generate_legal_moves(board, stateStack);
    if (legalMoves.empty()) {
        if (is_in_check(board, board.whiteToMove)) {
            return board.whiteToMove ? -kInfinity : kInfinity;
        }
        return 0;
    }

    if (maximizing) {
        int best = -kInfinity;
        for (const Move& move : legalMoves) {
            make_move(board, move, stateStack);
            const int score = minimax(board, depth - 1, alpha, beta, false, stateStack);
            undo_move(board, move, stateStack);

            if (score > best) best = score;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
        return best;
    }

    int best = kInfinity;
    for (const Move& move : legalMoves) {
        make_move(board, move, stateStack);
        const int score = minimax(board, depth - 1, alpha, beta, true, stateStack);
        undo_move(board, move, stateStack);

        if (score < best) best = score;
        if (best < beta) beta = best;
        if (beta <= alpha) break;
    }

    return best;
}

static char promotion_to_char(int promotionPiece) {
    switch (promotionPiece) {
        case 2: return 'n';
        case 3: return 'b';
        case 4: return 'r';
        case 6: return 'q';
        default: return 'q';
    }
}

void move_to_uci(const Move& move, char out[6]) {
    out[0] = static_cast<char>('a' + move.fromCol);
    out[1] = static_cast<char>('1' + move.fromRow);
    out[2] = static_cast<char>('a' + move.toCol);
    out[3] = static_cast<char>('1' + move.toRow);

    if (move.promotion != 0) {
        out[4] = promotion_to_char(move.promotion);
        out[5] = '\0';
    } else {
        out[4] = '\0';
    }
}

Move search_best_move(Board& board, int depth, std::vector<BoardState>& stateStack) {
    const std::vector<Move> legalMoves = generate_legal_moves(board, stateStack);
    if (legalMoves.empty()) {
        return Move{0, 0, 0, 0, 0, false, false};
    }

    const bool maximizing = board.whiteToMove;
    int bestScore = maximizing ? -kInfinity : kInfinity;
    Move bestMove = legalMoves[0];

    for (const Move& move : legalMoves) {
        make_move(board, move, stateStack);
        const int score = minimax(board, depth - 1, -kInfinity, kInfinity, !maximizing, stateStack);
        undo_move(board, move, stateStack);

        if (maximizing) {
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        } else {
            if (score < bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
    }

    return bestMove;
}
