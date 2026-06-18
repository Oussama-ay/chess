#include "chess/attack.h"

bool is_attacked_by(const Board& board, int targetRow, int targetCol, bool byWhite) {
    const int pawn   = byWhite ?  PAWN   : -PAWN;
    const int knight = byWhite ?  KNIGHT : -KNIGHT;
    const int bishop = byWhite ?  BISHOP : -BISHOP;
    const int rook   = byWhite ?  ROOK   : -ROOK;
    const int king   = byWhite ?  KING   : -KING;
    const int queen  = byWhite ?  QUEEN  : -QUEEN;

    // Pawn attacks
    const int pawnRow = byWhite ? targetRow - 1 : targetRow + 1;
    if (in_bounds(pawnRow, targetCol - 1) && board.squares[pawnRow][targetCol - 1] == pawn) return true;
    if (in_bounds(pawnRow, targetCol + 1) && board.squares[pawnRow][targetCol + 1] == pawn) return true;

    // Knight attacks
    static const int kKnightOffsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        { 1, -2}, { 1, 2}, { 2, -1}, { 2, 1}
    };
    for (const auto& off : kKnightOffsets) {
        const int r = targetRow + off[0];
        const int c = targetCol + off[1];
        if (in_bounds(r, c) && board.squares[r][c] == knight) return true;
    }

    // King attacks
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int r = targetRow + dr;
            const int c = targetCol + dc;
            if (in_bounds(r, c) && board.squares[r][c] == king) return true;
        }
    }

    static const int kDiagDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    for (const auto& dir : kDiagDirs) {
        int r = targetRow + dir[0];
        int c = targetCol + dir[1];
        while (in_bounds(r, c)) {
            int piece = board.squares[r][c];
            if (piece != 0) {
                if (piece == bishop || piece == queen) return true;
                break;
            }
            r += dir[0];
            c += dir[1];
        }
    }

    static const int kStraightDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& dir : kStraightDirs) {
        int r = targetRow + dir[0];
        int c = targetCol + dir[1];
        while (in_bounds(r, c)) {
            int piece = board.squares[r][c];
            if (piece != 0) {
                if (piece == rook || piece == queen) return true;
                break;
            }
            r += dir[0];
            c += dir[1];
        }
    }

    return false;
}

bool is_in_check(const Board& board, bool whiteKing) {
    const int kingPiece = whiteKing ? KING : -KING;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (board.squares[row][col] == kingPiece)
                return is_attacked_by(board, row, col, !whiteKing);

    return false;
}
