#include "chess/fen.h"
#include "chess/search.h"

#include <vector>

namespace {

constexpr int kMaxAllowedDepth = 12;

void normalize_limits(int& maxDepth, int& timeMs) {
    if (maxDepth < 1) maxDepth = 1;
    if (maxDepth > kMaxAllowedDepth) maxDepth = kMaxAllowedDepth;
    if (timeMs < 1) timeMs = 1;
}

} // namespace

extern "C" {

const char* get_best_move(const char* fen, int maxDepth, int timeMs) {
    static char uci[6] = "0000";
    normalize_limits(maxDepth, timeMs);

    Board board{};
    parse_fen(fen, board);

    std::vector<BoardState> stateStack;
    stateStack.reserve(256);

    Move best = search_best_move(board, maxDepth, timeMs, stateStack);
    move_to_uci(best, uci);
    return uci;
}

}
