#include "chess/fen.h"
#include "chess/search.h"

#include <iostream>
#include <vector>

extern "C" {

const char* get_best_move(const char* fen) {
    static char uci[6] = "0000";

    Board board{};
    parse_fen(fen, board);

    std::vector<BoardState> stateStack;
    stateStack.reserve(256);

    move_to_uci(search_best_move(board, 7, stateStack), uci);
    std::cout << uci << std::endl;
    return uci;
}

}
