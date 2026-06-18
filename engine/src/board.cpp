#include "chess/board.h"
#include "chess/zobrist.h"

void make_move(Board& board, const Move& move, std::vector<BoardState>& stateStack) {
    BoardState state{};
    state.captured      = board.squares[move.toRow][move.toCol];
    state.movedPiece    = board.squares[move.fromRow][move.fromCol];
    state.whiteToMove   = board.whiteToMove;
    state.enPassantCol  = board.enPassantCol;
    state.halfMoveClock = board.halfMoveClock;
    state.fullMoveNumber = board.fullMoveNumber;
    state.hash          = board.hash;
    for (int i = 0; i < 4; ++i) state.castlingRights[i] = board.castlingRights[i];
    stateStack.push_back(state);

    BoardState& last = stateStack.back();
    const int movingPiece = last.movedPiece;
    const bool whiteMoving = movingPiece > 0;
    const int pieceType = abs_piece(movingPiece);

    if (board.whiteToMove) board.hash ^= side_to_move_key();
    if (board.enPassantCol != -1) board.hash ^= en_passant_key(board.enPassantCol);
    xor_castling_rights(board.hash, board.castlingRights);

    // Remove moving piece from origin
    board.hash ^= piece_square_key(movingPiece, move.fromRow, move.fromCol);
    board.squares[move.fromRow][move.fromCol] = 0;

    if (move.isEnPassant) {
        last.captured = board.squares[move.fromRow][move.toCol];
        board.hash ^= piece_square_key(last.captured, move.fromRow, move.toCol);
        board.squares[move.fromRow][move.toCol] = 0;
    } else if (state.captured != 0) {
        board.hash ^= piece_square_key(state.captured, move.toRow, move.toCol);
    }

    int placedPiece = movingPiece;
    if (move.promotion != 0) placedPiece = whiteMoving ? move.promotion : -move.promotion;
    board.hash ^= piece_square_key(placedPiece, move.toRow, move.toCol);
    board.squares[move.toRow][move.toCol] = placedPiece;

    if (move.isCastling) {
        int rookFrom = (move.toCol == 6) ? 7 : 0;
        int rookTo   = (move.toCol == 6) ? 5 : 3;
        board.hash ^= piece_square_key(board.squares[move.toRow][rookFrom], move.toRow, rookFrom);
        board.squares[move.toRow][rookTo] = board.squares[move.toRow][rookFrom];
        board.squares[move.toRow][rookFrom] = 0;
        board.hash ^= piece_square_key(board.squares[move.toRow][rookTo], move.toRow, rookTo);
    }

    board.enPassantCol = -1;
    if (pieceType == PAWN && (move.toRow - move.fromRow == 2 || move.toRow - move.fromRow == -2))
        board.enPassantCol = move.fromCol;

    if (pieceType == KING) {
        int base = whiteMoving ? 0 : 2;
        board.castlingRights[base] = false;
        board.castlingRights[base + 1] = false;
    }

    if (pieceType == ROOK) {
        if (move.fromRow == 0 && move.fromCol == 0) board.castlingRights[1] = false;
        if (move.fromRow == 0 && move.fromCol == 7) board.castlingRights[0] = false;
        if (move.fromRow == 7 && move.fromCol == 0) board.castlingRights[3] = false;
        if (move.fromRow == 7 && move.fromCol == 7) board.castlingRights[2] = false;
    }

    if (last.captured != 0 && abs_piece(last.captured) == ROOK) {
        if (move.toRow == 0 && move.toCol == 0) board.castlingRights[1] = false;
        if (move.toRow == 0 && move.toCol == 7) board.castlingRights[0] = false;
        if (move.toRow == 7 && move.toCol == 0) board.castlingRights[3] = false;
        if (move.toRow == 7 && move.toCol == 7) board.castlingRights[2] = false;
    }

    board.halfMoveClock = (pieceType == PAWN || last.captured != 0) ? 0 : board.halfMoveClock + 1;
    if (!board.whiteToMove) board.fullMoveNumber += 1;
    board.whiteToMove = !board.whiteToMove;

    xor_castling_rights(board.hash, board.castlingRights);
    if (board.enPassantCol != -1) board.hash ^= en_passant_key(board.enPassantCol);
    if (board.whiteToMove) board.hash ^= side_to_move_key();
}

void undo_move(Board& board, const Move& move, std::vector<BoardState>& stateStack) {
    const BoardState& state = stateStack.back();

    if (move.isCastling) {
        int rookFrom = (move.toCol == 6) ? 7 : 0;
        int rookTo   = (move.toCol == 6) ? 5 : 3;
        board.squares[move.toRow][rookFrom] = board.squares[move.toRow][rookTo];
        board.squares[move.toRow][rookTo] = 0;
    }

    board.squares[move.fromRow][move.fromCol] = state.movedPiece;

    if (move.isEnPassant) {
        board.squares[move.toRow][move.toCol] = 0;
        board.squares[move.fromRow][move.toCol] = state.captured;
    } else {
        board.squares[move.toRow][move.toCol] = state.captured;
    }

    for (int i = 0; i < 4; ++i) board.castlingRights[i] = state.castlingRights[i];
    board.enPassantCol   = state.enPassantCol;
    board.halfMoveClock  = state.halfMoveClock;
    board.fullMoveNumber = state.fullMoveNumber;
    board.whiteToMove    = state.whiteToMove;
    board.hash           = state.hash;

    stateStack.pop_back();
}
