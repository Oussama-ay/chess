#include "chess/movegen.h"
#include "chess/attack.h"
#include "chess/board.h"

#include <cstddef>
#include <vector>

namespace {

static void add_pawn_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    const bool white = board.squares[row][col] > 0;
    const int dir = white ? 1 : -1;
    const int startRow = white ? 1 : 6;
    const int promoRow = white ? 7 : 0;
    const int epRow = white ? 4 : 3;
    static const int kPromos[4] = {KNIGHT, BISHOP, ROOK, QUEEN};

    int oneStep = row + dir;
    if (in_bounds(oneStep, col) && board.squares[oneStep][col] == 0) {
        if (oneStep == promoRow) {
            for (int p : kPromos) moves.push_back({row, col, oneStep, col, p, false, false});
        } else {
            moves.push_back({row, col, oneStep, col, 0, false, false});
            if (row == startRow) {
                int twoStep = row + 2 * dir;
                if (board.squares[twoStep][col] == 0)
                    moves.push_back({row, col, twoStep, col, 0, false, false});
            }
        }
    }

    for (int dc = -1; dc <= 1; dc += 2) {
        int cr = row + dir, cc = col + dc;
        if (!in_bounds(cr, cc)) continue;
        int target = board.squares[cr][cc];
        if (target != 0 && is_opponent_piece(target, white)) {
            if (cr == promoRow)
                for (int p : kPromos) moves.push_back({row, col, cr, cc, p, false, false});
            else
                moves.push_back({row, col, cr, cc, 0, false, false});
        }
    }

    if (board.enPassantCol != -1 && row == epRow) {
        if (board.enPassantCol == col - 1 || board.enPassantCol == col + 1)
            moves.push_back({row, col, row + dir, board.enPassantCol, 0, false, true});
    }
}

static void add_knight_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    static const int kOff[8][2] = {
        {-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}
    };
    bool white = board.squares[row][col] > 0;
    for (const auto& o : kOff) {
        int r = row + o[0], c = col + o[1];
        if (!in_bounds(r, c)) continue;
        int t = board.squares[r][c];
        if (t == 0 || is_opponent_piece(t, white))
            moves.push_back({row, col, r, c, 0, false, false});
    }
}

static void add_sliding_moves(const Board& board, int row, int col,
                               const int dirs[][2], int n, std::vector<Move>& moves) {
    bool white = board.squares[row][col] > 0;
    for (int i = 0; i < n; ++i) {
        int r = row + dirs[i][0], c = col + dirs[i][1];
        while (in_bounds(r, c)) {
            int t = board.squares[r][c];
            if (t == 0) {
                moves.push_back({row, col, r, c, 0, false, false});
            } else {
                if (is_opponent_piece(t, white))
                    moves.push_back({row, col, r, c, 0, false, false});
                break;
            }
            r += dirs[i][0]; c += dirs[i][1];
        }
    }
}

static void add_king_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    bool white = board.squares[row][col] > 0;

    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            int r = row + dr, c = col + dc;
            if (!in_bounds(r, c)) continue;
            int t = board.squares[r][c];
            if (t == 0 || is_opponent_piece(t, white))
                moves.push_back({row, col, r, c, 0, false, false});
        }

    int baseRow = white ? 0 : 7;
    int ks = white ? 0 : 2;
    int rook = white ? ROOK : -ROOK;
    if (row == baseRow && col == 4) {
        if (board.castlingRights[ks] &&
            board.squares[baseRow][7] == rook &&
            board.squares[baseRow][5] == 0 &&
            board.squares[baseRow][6] == 0)
            moves.push_back({baseRow, 4, baseRow, 6, 0, true, false});
        if (board.castlingRights[ks+1] &&
            board.squares[baseRow][0] == rook &&
            board.squares[baseRow][1] == 0 &&
            board.squares[baseRow][2] == 0 &&
            board.squares[baseRow][3] == 0)
            moves.push_back({baseRow, 4, baseRow, 2, 0, true, false});
    }
}

static void generate_pseudo_legal(const Board& board, std::vector<Move>& moves) {
    static const int kBishopDirs[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
    static const int kRookDirs[4][2]   = {{-1,0},{1,0},{0,-1},{0,1}};
    static const int kQueenDirs[8][2]  = {{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            int piece = board.squares[row][col];
            if (piece == 0 || !is_side_piece(piece, board.whiteToMove)) continue;
            switch (abs_piece(piece)) {
                case PAWN:   add_pawn_moves(board, row, col, moves);                    break;
                case KNIGHT: add_knight_moves(board, row, col, moves);                  break;
                case BISHOP: add_sliding_moves(board, row, col, kBishopDirs, 4, moves); break;
                case ROOK:   add_sliding_moves(board, row, col, kRookDirs, 4, moves);   break;
                case KING:   add_king_moves(board, row, col, moves);                    break;
                case QUEEN:  add_sliding_moves(board, row, col, kQueenDirs, 8, moves);  break;
            }
        }
}

} // namespace

void generate_legal_moves(Board& board, std::vector<BoardState>& stateStack, std::vector<Move>& moves) {
    moves.clear();
    generate_pseudo_legal(board, moves);

    bool side = board.whiteToMove;
    bool inCheck = is_in_check(board, side);

    std::size_t write = 0;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const Move& move = moves[i];

        if (move.isCastling) {
            if (inCheck) continue;
            int mid = (move.toCol == 6) ? 5 : 3;
            if (is_attacked_by(board, move.fromRow, mid, !side)) continue;
            if (is_attacked_by(board, move.fromRow, move.toCol, !side)) continue;
        }

        make_move(board, move, stateStack);
        bool legal = !is_in_check(board, side);
        undo_move(board, move, stateStack);

        if (legal) moves[write++] = move;
    }
    moves.resize(write);
}
