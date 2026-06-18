#pragma once

#include "chess/types.h"

#include <cstdint>

std::uint64_t piece_square_key(int piece, int row, int col);
std::uint64_t side_to_move_key();
std::uint64_t castling_key(int index);
std::uint64_t en_passant_key(int file);

void          xor_castling_rights(std::uint64_t& hash, const bool castlingRights[4]);
std::uint64_t compute_zobrist_hash(const Board& board);
