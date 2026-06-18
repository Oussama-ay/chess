#pragma once

#include "chess/types.h"

bool is_attacked_by(const Board& board, int targetRow, int targetCol, bool byWhite);
bool is_in_check(const Board& board, bool whiteKing);
