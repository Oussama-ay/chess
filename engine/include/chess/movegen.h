#pragma once

#include "chess/types.h"

#include <vector>

void generate_legal_moves(Board& board, std::vector<BoardState>& stateStack, std::vector<Move>& outMoves);

