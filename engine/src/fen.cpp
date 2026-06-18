#include "chess/fen.h"
#include "chess/zobrist.h"

#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

static int piece_from_char(char c) {
    bool white = std::isupper(static_cast<unsigned char>(c)) != 0;
    int piece = 0;
    switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'p': piece = PAWN;   break;
        case 'n': piece = KNIGHT; break;
        case 'b': piece = BISHOP; break;
        case 'r': piece = ROOK;   break;
        case 'k': piece = KING;   break;
        case 'q': piece = QUEEN;  break;
        default: return 0;
    }
    return white ? piece : -piece;
}

void parse_fen(const char* fen, Board& out) {
    std::memset(&out, 0, sizeof(out));
    out.enPassantCol = -1;

    std::istringstream ss(fen);
    std::string tok[6];
    for (int i = 0; i < 6; ++i) ss >> tok[i];

    int row = 7, col = 0;
    for (char c : tok[0]) {
        if (c == '/') { --row; col = 0; }
        else if (std::isdigit(static_cast<unsigned char>(c))) col += c - '0';
        else { out.squares[row][col] = piece_from_char(c); ++col; }
    }

    out.whiteToMove = (tok[1] == "w");
    out.castlingRights[0] = tok[2].find('K') != std::string::npos;
    out.castlingRights[1] = tok[2].find('Q') != std::string::npos;
    out.castlingRights[2] = tok[2].find('k') != std::string::npos;
    out.castlingRights[3] = tok[2].find('q') != std::string::npos;
    out.enPassantCol = (tok[3] == "-") ? -1 : tok[3][0] - 'a';
    out.halfMoveClock = std::stoi(tok[4]);
    out.fullMoveNumber = std::stoi(tok[5]);
    out.hash = compute_zobrist_hash(out);
}
