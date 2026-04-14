#include "chess/fen.h"
#include "chess/search.h"

#include <vector>
#include <iostream>
extern "C" {

const char* get_best_move(const char* fen) {
    static char bestMoveUci[6] = "0000";
    static constexpr int kSearchDepth = 6;

    Board board{};
    parse_fen(fen, board);

    std::vector<BoardState> stateStack;
    stateStack.reserve(256);

    const Move bestMove = search_best_move(board, kSearchDepth, stateStack);
    move_to_uci(bestMove, bestMoveUci);
    std::cout << bestMoveUci << std::endl;
    return bestMoveUci;
}

} // extern "C"
