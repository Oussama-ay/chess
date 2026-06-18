#pragma once

#include "chess/types.h"

#include <vector>

Move search_best_move(Board& board, int depth, std::vector<BoardState>& stateStack);
void move_to_uci(const Move& move, char out[6]);
