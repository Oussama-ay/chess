#include "chess/movegen.h"

void    make_move(Board& board, const Move& move, std::vector<BoardState>& stateStack) {
    BoardState state{};
    state.captured = board.squares[move.toRow][move.toCol];
    state.movedPiece = board.squares[move.fromRow][move.fromCol];
    state.whiteToMove = board.whiteToMove;
    for (int i = 0; i < 4; ++i) {
        state.castlingRights[i] = board.castlingRights[i];
    }
    state.enPassantCol = board.enPassantCol;
    state.halfMoveClock = board.halfMoveClock;
    state.fullMoveNumber = board.fullMoveNumber;
    stateStack.push_back(state);

    BoardState& last = stateStack.back();
    const int movingPiece = last.movedPiece;
    const bool whiteMoving = movingPiece > 0;
    const int pieceType = abs_piece(movingPiece);

    board.squares[move.fromRow][move.fromCol] = 0;

    if (move.isEnPassant) {
        const int capturedRow = move.fromRow;
        last.captured = board.squares[capturedRow][move.toCol];
        board.squares[capturedRow][move.toCol] = 0;
    }

    int placedPiece = movingPiece;
    if (move.promotion != 0) {
        placedPiece = whiteMoving ? move.promotion : -move.promotion;
    }
    board.squares[move.toRow][move.toCol] = placedPiece;

    if (move.isCastling) {
        if (move.toCol == 6) {
            board.squares[move.toRow][5] = board.squares[move.toRow][7];
            board.squares[move.toRow][7] = 0;
        } else {
            board.squares[move.toRow][3] = board.squares[move.toRow][0];
            board.squares[move.toRow][0] = 0;
        }
    }

    board.enPassantCol = -1;
    if (pieceType == 1 && (move.toRow - move.fromRow == 2 || move.toRow - move.fromRow == -2)) {
        board.enPassantCol = move.fromCol;
    }

    if (pieceType == 5) {
        if (whiteMoving) {
            board.castlingRights[0] = false;
            board.castlingRights[1] = false;
        } else {
            board.castlingRights[2] = false;
            board.castlingRights[3] = false;
        }
    }

    if (pieceType == 4) {
        if (move.fromRow == 0 && move.fromCol == 0) board.castlingRights[1] = false;
        if (move.fromRow == 0 && move.fromCol == 7) board.castlingRights[0] = false;
        if (move.fromRow == 7 && move.fromCol == 0) board.castlingRights[3] = false;
        if (move.fromRow == 7 && move.fromCol == 7) board.castlingRights[2] = false;
    }

    if (last.captured != 0 && abs_piece(last.captured) == 4) {
        if (move.toRow == 0 && move.toCol == 0) board.castlingRights[1] = false;
        if (move.toRow == 0 && move.toCol == 7) board.castlingRights[0] = false;
        if (move.toRow == 7 && move.toCol == 0) board.castlingRights[3] = false;
        if (move.toRow == 7 && move.toCol == 7) board.castlingRights[2] = false;
    }

    if (pieceType == 1 || last.captured != 0) {
        board.halfMoveClock = 0;
    } else {
        board.halfMoveClock += 1;
    }

    if (!board.whiteToMove) {
        board.fullMoveNumber += 1;
    }

    board.whiteToMove = !board.whiteToMove;
}

void undo_move(Board& board, const Move& move, std::vector<BoardState>& stateStack) {
    const BoardState state = stateStack.back();
    stateStack.pop_back();

    if (move.isCastling) {
        if (move.toCol == 6) {
            board.squares[move.toRow][7] = board.squares[move.toRow][5];
            board.squares[move.toRow][5] = 0;
        } else {
            board.squares[move.toRow][0] = board.squares[move.toRow][3];
            board.squares[move.toRow][3] = 0;
        }
    }

    board.squares[move.fromRow][move.fromCol] = state.movedPiece;

    if (move.isEnPassant) {
        const int capturedRow = move.fromRow;
        board.squares[move.toRow][move.toCol] = 0;
        board.squares[capturedRow][move.toCol] = state.captured;
    } else {
        board.squares[move.toRow][move.toCol] = state.captured;
    }

    for (int i = 0; i < 4; ++i) {
        board.castlingRights[i] = state.castlingRights[i];
    }
    board.enPassantCol = state.enPassantCol;
    board.halfMoveClock = state.halfMoveClock;
    board.fullMoveNumber = state.fullMoveNumber;
    board.whiteToMove = state.whiteToMove;
}

bool is_attacked_by(const Board& board, int targetRow, int targetCol, bool byWhite) {
    const int pawn = byWhite ? 1 : -1;
    const int knight = byWhite ? 2 : -2;
    const int bishop = byWhite ? 3 : -3;
    const int rook = byWhite ? 4 : -4;
    const int king = byWhite ? 5 : -5;
    const int queen = byWhite ? 6 : -6;

    const int pawnRow = byWhite ? targetRow - 1 : targetRow + 1;
    if (in_bounds(pawnRow, targetCol - 1) && board.squares[pawnRow][targetCol - 1] == pawn) return true;
    if (in_bounds(pawnRow, targetCol + 1) && board.squares[pawnRow][targetCol + 1] == pawn) return true;

    static const int kKnightOffsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    for (const auto& off : kKnightOffsets) {
        const int r = targetRow + off[0];
        const int c = targetCol + off[1];
        if (in_bounds(r, c) && board.squares[r][c] == knight) return true;
    }

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int r = targetRow + dr;
            const int c = targetCol + dc;
            if (in_bounds(r, c) && board.squares[r][c] == king) return true;
        }
    }

    static const int kDiagDirs[4][2] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };
    for (const auto& dir : kDiagDirs) {
        int r = targetRow + dir[0];
        int c = targetCol + dir[1];
        while (in_bounds(r, c)) {
            const int piece = board.squares[r][c];
            if (piece != 0) {
                if (piece == bishop || piece == queen) return true;
                break;
            }
            r += dir[0];
            c += dir[1];
        }
    }

    static const int kStraightDirs[4][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    for (const auto& dir : kStraightDirs) {
        int r = targetRow + dir[0];
        int c = targetCol + dir[1];
        while (in_bounds(r, c)) {
            const int piece = board.squares[r][c];
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
    const int kingPiece = whiteKing ? 5 : -5;
    int kingRow = -1;
    int kingCol = -1;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (board.squares[row][col] == kingPiece) {
                kingRow = row;
                kingCol = col;
                break;
            }
        }
        if (kingRow != -1) break;
    }

    return is_attacked_by(board, kingRow, kingCol, !whiteKing);
}

static void add_pawn_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    const int piece = board.squares[row][col];
    const bool white = piece > 0;
    const int dir = white ? 1 : -1;
    const int startRow = white ? 1 : 6;
    const int promotionRow = white ? 7 : 0;
    const int enPassantRow = white ? 4 : 3;
    static const int kPromotionPieces[4] = {2, 3, 4, 6};

    const int oneStepRow = row + dir;
    if (in_bounds(oneStepRow, col) && board.squares[oneStepRow][col] == 0) {
        if (oneStepRow == promotionRow) {
            for (int promo : kPromotionPieces) {
                moves.push_back({row, col, oneStepRow, col, promo, false, false});
            }
        } else {
            moves.push_back({row, col, oneStepRow, col, 0, false, false});
            if (row == startRow) {
                const int twoStepRow = row + (2 * dir);
                if (board.squares[twoStepRow][col] == 0) {
                    moves.push_back({row, col, twoStepRow, col, 0, false, false});
                }
            }
        }
    }

    for (int dc = -1; dc <= 1; dc += 2) {
        const int captureCol = col + dc;
        const int captureRow = row + dir;
        if (!in_bounds(captureRow, captureCol)) continue;

        const int target = board.squares[captureRow][captureCol];
        if (target != 0 && is_opponent_piece(target, white)) {
            if (captureRow == promotionRow) {
                for (int promo : kPromotionPieces) {
                    moves.push_back({row, col, captureRow, captureCol, promo, false, false});
                }
            } else {
                moves.push_back({row, col, captureRow, captureCol, 0, false, false});
            }
        }
    }

    if (board.enPassantCol != -1 && row == enPassantRow) {
        if (board.enPassantCol == col - 1 || board.enPassantCol == col + 1) {
            const int targetCol = board.enPassantCol;
            const int targetRow = row + dir;
            moves.push_back({row, col, targetRow, targetCol, 0, false, true});
        }
    }
}

static void add_knight_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    static const int kOffsets[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };

    const bool white = board.squares[row][col] > 0;
    for (const auto& off : kOffsets) {
        const int r = row + off[0];
        const int c = col + off[1];
        if (!in_bounds(r, c)) continue;

        const int target = board.squares[r][c];
        if (target == 0 || is_opponent_piece(target, white)) {
            moves.push_back({row, col, r, c, 0, false, false});
        }
    }
}

static void add_sliding_moves(
    const Board& board,
    int row,
    int col,
    const int dirs[][2],
    int dirCount,
    std::vector<Move>& moves
) {
    const bool white = board.squares[row][col] > 0;
    for (int i = 0; i < dirCount; ++i) {
        int r = row + dirs[i][0];
        int c = col + dirs[i][1];

        while (in_bounds(r, c)) {
            const int target = board.squares[r][c];
            if (target == 0) {
                moves.push_back({row, col, r, c, 0, false, false});
            } else {
                if (is_opponent_piece(target, white)) {
                    moves.push_back({row, col, r, c, 0, false, false});
                }
                break;
            }

            r += dirs[i][0];
            c += dirs[i][1];
        }
    }
}

static void add_king_moves(const Board& board, int row, int col, std::vector<Move>& moves) {
    const bool white = board.squares[row][col] > 0;

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;

            const int r = row + dr;
            const int c = col + dc;
            if (!in_bounds(r, c)) continue;

            const int target = board.squares[r][c];
            if (target == 0 || is_opponent_piece(target, white)) {
                moves.push_back({row, col, r, c, 0, false, false});
            }
        }
    }

    if (white && row == 0 && col == 4) {
        if (board.castlingRights[0] && board.squares[0][5] == 0 && board.squares[0][6] == 0) {
            moves.push_back({0, 4, 0, 6, 0, true, false});
        }
        if (board.castlingRights[1] && board.squares[0][1] == 0 && board.squares[0][2] == 0 && board.squares[0][3] == 0) {
            moves.push_back({0, 4, 0, 2, 0, true, false});
        }
    }

    if (!white && row == 7 && col == 4) {
        if (board.castlingRights[2] && board.squares[7][5] == 0 && board.squares[7][6] == 0) {
            moves.push_back({7, 4, 7, 6, 0, true, false});
        }
        if (board.castlingRights[3] && board.squares[7][1] == 0 && board.squares[7][2] == 0 && board.squares[7][3] == 0) {
            moves.push_back({7, 4, 7, 2, 0, true, false});
        }
    }
}

static std::vector<Move> generate_pseudo_legal_moves(const Board& board) {
    std::vector<Move> moves;
    const bool whiteToMove = board.whiteToMove;

    static const int kBishopDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    static const int kRookDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    static const int kQueenDirs[8][2] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int piece = board.squares[row][col];
            if (piece == 0 || !is_side_piece(piece, whiteToMove)) continue;

            switch (abs_piece(piece)) {
                case 1: add_pawn_moves(board, row, col, moves); break;
                case 2: add_knight_moves(board, row, col, moves); break;
                case 3: add_sliding_moves(board, row, col, kBishopDirs, 4, moves); break;
                case 4: add_sliding_moves(board, row, col, kRookDirs, 4, moves); break;
                case 5: add_king_moves(board, row, col, moves); break;
                case 6: add_sliding_moves(board, row, col, kQueenDirs, 8, moves); break;
            }
        }
    }

    return moves;
}

std::vector<Move> generate_legal_moves(Board& board, std::vector<BoardState>& stateStack) {
    std::vector<Move> legalMoves;
    const std::vector<Move> pseudoMoves = generate_pseudo_legal_moves(board);
    const bool sideToMove = board.whiteToMove;

    for (const Move& move : pseudoMoves) {
        if (move.isCastling) {
            const bool opponent = !sideToMove;
            if (is_in_check(board, sideToMove)) continue;

            const int midCol = (move.toCol == 6) ? 5 : 3;
            if (is_attacked_by(board, move.fromRow, midCol, opponent)) continue;
            if (is_attacked_by(board, move.fromRow, move.toCol, opponent)) continue;
        }

        make_move(board, move, stateStack);
        const bool kingInCheck = is_in_check(board, sideToMove);
        undo_move(board, move, stateStack);

        if (!kingInCheck) {
            legalMoves.push_back(move);
        }
    }

    return legalMoves;
}
