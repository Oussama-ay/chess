#include "chess/fen.h"

#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

static int piece_from_fen_char(char c) {
    const bool isWhite = std::isupper(static_cast<unsigned char>(c)) != 0;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    int piece = 0;

    switch (lower) {
        case 'p': piece = 1; break;
        case 'n': piece = 2; break;
        case 'b': piece = 3; break;
        case 'r': piece = 4; break;
        case 'k': piece = 5; break;
        case 'q': piece = 6; break;
        default: return 0;
    }

    return isWhite ? piece : -piece;
}

void parse_fen(const char* fen, Board& outBoard) {
    std::memset(&outBoard, 0, sizeof(outBoard));
    outBoard.enPassantCol = -1;

    std::istringstream ss(fen);
    std::array<std::string, 6> tokens;
    for (int i = 0; i < 6; ++i) {
        ss >> tokens[i];
    }

    int row = 7;
    int col = 0;
    for (char c : tokens[0]) {
        if (c == '/') {
            --row;
            col = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            const int skip = c - '0';
            col += skip;
            continue;
        }

        outBoard.squares[row][col] = piece_from_fen_char(c);
        ++col;
    }

    outBoard.whiteToMove = (tokens[1] == "w");

    outBoard.castlingRights[0] = tokens[2].find('K') != std::string::npos;
    outBoard.castlingRights[1] = tokens[2].find('Q') != std::string::npos;
    outBoard.castlingRights[2] = tokens[2].find('k') != std::string::npos;
    outBoard.castlingRights[3] = tokens[2].find('q') != std::string::npos;

    if (tokens[3] == "-") {
        outBoard.enPassantCol = -1;
    } else {
        outBoard.enPassantCol = tokens[3][0] - 'a';
    }

    outBoard.halfMoveClock = std::stoi(tokens[4]);
    outBoard.fullMoveNumber = std::stoi(tokens[5]);
}
