#include "chess/search.h"

#include "chess/eval.h"
#include "chess/movegen.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

static constexpr int kMateScore = kInfinity - 1000;
static constexpr std::size_t kTTSize = 1u << 18;

enum class TTFlag : std::uint8_t {
    Exact,
    LowerBound,
    UpperBound,
};

struct TTEntry {
    std::uint64_t hash = 0;
    int depth = -1;
    int score = 0;
    TTFlag flag = TTFlag::Exact;
    Move bestMove{0, 0, 0, 0, 0, false, false};
    bool valid = false;
};

struct ScoredMove {
    int score;
    Move move;
};

static std::vector<TTEntry> g_transpositionTable(kTTSize);

static bool same_move(const Move& a, const Move& b)
{
    return a.fromRow == b.fromRow && a.fromCol == b.fromCol &&
           a.toRow == b.toRow && a.toCol == b.toCol &&
           a.promotion == b.promotion &&
           a.isCastling == b.isCastling &&
           a.isEnPassant == b.isEnPassant;
}

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

static void order_moves(const Board& board, std::vector<Move>& moves, const Move* preferredMove)
{
    std::vector<ScoredMove> scored;
    scored.reserve(moves.size());

    for (const Move& move : moves) {
        int score = move_ordering_score(board, move);
        if (preferredMove != nullptr && same_move(move, *preferredMove)) {
            score += 1000000;
        }
        scored.push_back({score, move});
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    for (std::size_t i = 0; i < scored.size(); ++i) {
        moves[i] = scored[i].move;
    }
}

static std::uint64_t tt_index(std::uint64_t hash)
{
    return hash & static_cast<std::uint64_t>(kTTSize - 1);
}

static TTEntry* probe_tt(std::uint64_t hash)
{
    TTEntry& entry = g_transpositionTable[static_cast<std::size_t>(tt_index(hash))];
    if (entry.valid && entry.hash == hash) {
        return &entry;
    }

    return nullptr;
}

static void store_tt(std::uint64_t hash, int depth, int score, TTFlag flag, const Move& bestMove)
{
    TTEntry& entry = g_transpositionTable[static_cast<std::size_t>(tt_index(hash))];
    if (!entry.valid || depth >= entry.depth || entry.hash != hash) {
        entry.valid = true;
        entry.hash = hash;
        entry.depth = depth;
        entry.score = score;
        entry.flag = flag;
        entry.bestMove = bestMove;
    }
}

static int score_to_tt(int score, int ply)
{
    if (score >= kMateScore - 1000) {
        return score + ply;
    }

    if (score <= -kMateScore + 1000) {
        return score - ply;
    }

    return score;
}

static int score_from_tt(int score, int ply)
{
    if (score >= kMateScore - 1000) {
        return score - ply;
    }

    if (score <= -kMateScore + 1000) {
        return score + ply;
    }

    return score;
}

static int negamax(Board& board, int depth, int alpha, int beta, std::vector<BoardState>& stateStack, int ply)
{
    if (board.halfMoveClock >= 100)
        return 0;

    const int originalAlpha = alpha;
    const int originalBeta = beta;

    TTEntry* ttEntry = probe_tt(board.hash);
    Move ttMove{0, 0, 0, 0, 0, false, false};
    if (ttEntry != nullptr) {
        ttMove = ttEntry->bestMove;
        if (ttEntry->depth >= depth) {
            const int ttScore = score_from_tt(ttEntry->score, ply);
            switch (ttEntry->flag) {
                case TTFlag::Exact:
                    return ttScore;
                case TTFlag::LowerBound:
                    if (ttScore >= beta) return ttScore;
                    alpha = std::max(alpha, ttScore);
                    break;
                case TTFlag::UpperBound:
                    if (ttScore <= alpha) return ttScore;
                    beta = std::min(beta, ttScore);
                    break;
            }
        }
    }

    if (depth == 0)
    {
        const int eval = evaluate(board);
        return board.whiteToMove ? eval : -eval;
    }

    std::vector<Move> legalMoves;
    legalMoves.reserve(64);
    generate_legal_moves(board, stateStack, legalMoves);
    if (legalMoves.empty())
    {
        if (is_in_check(board, board.whiteToMove))
            return -kMateScore + ply;
        return 0;
    }

    order_moves(board, legalMoves, ttEntry != nullptr ? &ttMove : nullptr);

    int best = -kInfinity;
    Move bestMove = legalMoves.front();

    for (const Move& move : legalMoves)
    {
        make_move(board, move, stateStack);
        const int score = -negamax(board, depth - 1, -beta, -alpha, stateStack, ply + 1);
        undo_move(board, move, stateStack);

        if (score > best) {
            best = score;
            bestMove = move;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }

    TTFlag flag = TTFlag::Exact;
    if (best <= originalAlpha) {
        flag = TTFlag::UpperBound;
    } else if (best >= originalBeta) {
        flag = TTFlag::LowerBound;
    }

    store_tt(board.hash, depth, score_to_tt(best, ply), flag, bestMove);
    return best;
}

} // namespace

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
    std::vector<Move> legalMoves;
    legalMoves.reserve(64);
    generate_legal_moves(board, stateStack, legalMoves);
    if (legalMoves.empty()) {
        return Move{0, 0, 0, 0, 0, false, false};
    }

    Move bestMove = legalMoves.front();
    Move preferredMove = bestMove;
    if (TTEntry* entry = probe_tt(board.hash)) {
        preferredMove = entry->bestMove;
    }

    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        std::vector<Move> orderedMoves = legalMoves;
        order_moves(board, orderedMoves, &preferredMove);

        int alpha = -kInfinity;
        const int beta = kInfinity;
        int iterationBestScore = -kInfinity;
        Move iterationBestMove = orderedMoves.front();

        for (const Move& move : orderedMoves) {
            make_move(board, move, stateStack);
            const int score = -negamax(board, currentDepth - 1, -beta, -alpha, stateStack, 1);
            undo_move(board, move, stateStack);

            if (score > iterationBestScore) {
                iterationBestScore = score;
                iterationBestMove = move;
            }

            if (score > alpha) alpha = score;
        }

        store_tt(board.hash, currentDepth, score_to_tt(iterationBestScore, 0), TTFlag::Exact, iterationBestMove);

        bestMove = iterationBestMove;
        preferredMove = bestMove;
    }

    return bestMove;
}
