#pragma once

struct Board {
    int squares[8][8];      // +N = white, -N = black, 0 = empty
    bool whiteToMove;
    bool castlingRights[4]; // [WK, WQ, BK, BQ]
    int enPassantCol;       // -1 if none
    int halfMoveClock;
    int fullMoveNumber;
};

struct Move {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
    int promotion;  // 0 = none, or piece type (2-6)
    bool isCastling;
    bool isEnPassant;
};

struct BoardState {
    int captured;
    bool castlingRights[4];
    int enPassantCol;
    int halfMoveClock;
    int fullMoveNumber;
    int movedPiece;
    bool whiteToMove;
};

inline bool in_bounds(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

inline int abs_piece(int piece) {
    return piece < 0 ? -piece : piece;
}

inline bool is_white_piece(int piece) {
    return piece > 0;
}

inline bool is_black_piece(int piece) {
    return piece < 0;
}

inline bool is_side_piece(int piece, bool whiteSide) {
    return whiteSide ? is_white_piece(piece) : is_black_piece(piece);
}

inline bool is_opponent_piece(int piece, bool whiteSide) {
    return whiteSide ? is_black_piece(piece) : is_white_piece(piece);
}
