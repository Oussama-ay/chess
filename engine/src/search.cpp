#include "chess/search.h"

#include "chess/eval.h"
#include "chess/movegen.h"

static constexpr int kMateScore = kInfinity - 1000;

static int piece_value_for_ordering(int pieceType)
{
    switch (pieceType)
    {
        case 1: return 100;
        case 2: return 320;
        case 3: return 330;
        case 4: return 500;
        case 5: return 20000;
        case 6: return 900;
        default: return 0;
    }
}

static int move_ordering_score(const Board& board, const Move& move)
{
    const int movingPiece = board.squares[move.fromRow][move.fromCol];
    const int movingValue = piece_value_for_ordering(abs_piece(movingPiece));

    int score = 0;
    int capturedPiece = board.squares[move.toRow][move.toCol];
    if (move.isEnPassant) {
        capturedPiece = board.whiteToMove ? -1 : 1;
    }

    if (capturedPiece != 0) {
        const int capturedValue = piece_value_for_ordering(abs_piece(capturedPiece));
        score += 10000 + (capturedValue * 16) - movingValue;
    }

    if (move.promotion != 0) {
        score += 8000 + piece_value_for_ordering(move.promotion);
    }

    if (move.isCastling) {
        score += 50;
    }

    return score;
}

static void order_moves(const Board& board, std::vector<Move>& moves)
{
    std::sort(moves.begin(), moves.end(), [&board](const Move& a, const Move& b) {
        return move_ordering_score(board, a) > move_ordering_score(board, b);
    });
}

static int negamax(Board& board, int depth, int alpha, int beta, std::vector<BoardState>& stateStack, int ply)
{
    if (board.halfMoveClock >= 100)
        return 0;

    if (depth == 0)
    {
        const int eval = evaluate(board);
        return board.whiteToMove ? eval : -eval;
    }

    std::vector<Move> legalMoves = generate_legal_moves(board, stateStack);
    if (legalMoves.empty())
    {
        if (is_in_check(board, board.whiteToMove))
            return -kMateScore + ply;
        return 0;
    }

    order_moves(board, legalMoves);

    int best = -kInfinity;
    for (const Move& move : legalMoves)
    {
        make_move(board, move, stateStack);
        const int score = -negamax(board, depth - 1, -beta, -alpha, stateStack, ply + 1);
        undo_move(board, move, stateStack);

        if (score > best) best = score;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }

    return best;
}

static bool is_null_move(const Move& move)
{
    return move.fromRow == 0 && move.fromCol == 0 &&
           move.toRow == 0 && move.toCol == 0 &&
           move.promotion == 0 && !move.isCastling && !move.isEnPassant;
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
    if (is_null_move(move)) {
        out[0] = '0';
        out[1] = '0';
        out[2] = '0';
        out[3] = '0';
        out[4] = '\0';
        return;
    }

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
    std::vector<Move> legalMoves = generate_legal_moves(board, stateStack);
    if (legalMoves.empty()) {
        return Move{0, 0, 0, 0, 0, false, false};
    }

    order_moves(board, legalMoves);

    int bestScore = -kInfinity;
    Move bestMove = legalMoves[0];

    for (const Move& move : legalMoves) {
        make_move(board, move, stateStack);
        const int score = -negamax(board, depth - 1, -kInfinity, kInfinity, stateStack, 1);
        undo_move(board, move, stateStack);

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}
