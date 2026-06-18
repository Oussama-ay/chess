#pragma once

#include "chess/types.h"

#include <vector>

void make_move(Board& board, const Move& move, std::vector<BoardState>& stateStack);
void undo_move(Board& board, const Move& move, std::vector<BoardState>& stateStack);
