#pragma once

#include "chess/types.h"

#include <vector>

void make_move(Board& board, const Move& move, std::vector<BoardState>& stateStack);
void undo_move(Board& board, const Move& move, std::vector<BoardState>& stateStack);

bool is_attacked_by(const Board& board, int targetRow, int targetCol, bool byWhite);
bool is_in_check(const Board& board, bool whiteKing);

std::vector<Move> generate_legal_moves(Board& board, std::vector<BoardState>& stateStack);
