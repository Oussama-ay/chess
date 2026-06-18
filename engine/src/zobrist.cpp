#include "chess/zobrist.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

static std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static const std::array<std::uint64_t, 64 * 12>& all_piece_square_keys() {
    static const std::array<std::uint64_t, 64 * 12> keys = [] {
        std::array<std::uint64_t, 64 * 12> out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = splitmix64(0xA17C9E3779B97F4AULL + static_cast<std::uint64_t>(i));
        }
        return out;
    }();
    return keys;
}

static int piece_index(int piece) {
    const int type = abs_piece(piece) - 1;
    const int colorOffset = piece > 0 ? 0 : 6;
    return colorOffset + type;
}

} // namespace

std::uint64_t piece_square_key(int piece, int row, int col) {
    const int index = piece_index(piece) * 64 + (row * 8 + col);
    return all_piece_square_keys()[static_cast<std::size_t>(index)];
}

std::uint64_t side_to_move_key() {
    static const std::uint64_t key = splitmix64(0xD1B54A32D192ED03ULL);
    return key;
}

std::uint64_t castling_key(int index) {
    static const std::array<std::uint64_t, 4> keys = [] {
        std::array<std::uint64_t, 4> out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = splitmix64(0xE7037ED1A0B428DBULL + static_cast<std::uint64_t>(i));
        }
        return out;
    }();
    return keys[static_cast<std::size_t>(index)];
}

std::uint64_t en_passant_key(int file) {
    static const std::array<std::uint64_t, 8> keys = [] {
        std::array<std::uint64_t, 8> out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = splitmix64(0x8CB92BA72F3D8DD7ULL + static_cast<std::uint64_t>(i));
        }
        return out;
    }();
    return keys[static_cast<std::size_t>(file)];
}

void xor_castling_rights(std::uint64_t& hash, const bool castlingRights[4]) {
    for (int i = 0; i < 4; ++i) {
        if (castlingRights[i]) {
            hash ^= castling_key(i);
        }
    }
}

std::uint64_t compute_zobrist_hash(const Board& board) {
    std::uint64_t hash = 0;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int piece = board.squares[row][col];
            if (piece != 0) {
                hash ^= piece_square_key(piece, row, col);
            }
        }
    }

    xor_castling_rights(hash, board.castlingRights);

    if (board.enPassantCol != -1) {
        hash ^= en_passant_key(board.enPassantCol);
    }

    if (board.whiteToMove) {
        hash ^= side_to_move_key();
    }

    return hash;
}
