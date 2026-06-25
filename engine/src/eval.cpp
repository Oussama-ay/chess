#include "chess/eval.h"

#include "chess/types.h"

static const int kPieceValue[7] = {
	0,
	100,	// pawn
	320,	// knight
	350,	// bishop
	500,	// rook
	20000,  // king
	900  // queen
};

static const int kPieceSquareTable[7][64] = {
	// empty
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	},
	// pawn
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		50, 50, 50, 50, 50, 50, 50, 50,
		10, 10, 20, 30, 30, 20, 10, 10,
		5, 5, 10, 25, 25, 10, 5, 5,
		0, 0, 0, 20, 20, 0, 0, 0,
		5, -5, -10, 0, 0, -10, -5, 5,
		5, 10, 10, -20, -20, 10, 10, 5,
		0, 0, 0, 0, 0, 0, 0, 0
	},
	// knight
	{
		-50, -40, -30, -30, -30, -30, -40, -50,
		-40, -20, 0, 0, 0, 0, -20, -40,
		-30, 0, 10, 15, 15, 10, 0, -30,
		-30, 5, 15, 20, 20, 15, 5, -30,
		-30, 0, 15, 20, 20, 15, 0, -30,
		-30, 5, 10, 15, 15, 10, 5, -30,
		-40, -20, 0, 5, 5, 0, -20, -40,
		-50, -40, -30, -30, -30, -30, -40, -50
	},
	// bishop
	{
		-20, -10, -10, -10, -10, -10, -10, -20,
		-10, 5, 0, 0, 0, 0, 5, -10,
		-10, 10, 10, 10, 10, 10, 10, -10,
		-10, 0, 10, 10, 10, 10, 0, -10,
		-10, 5, 5, 10, 10, 5, 5, -10,
		-10, 0, 5, 10, 10, 5, 0, -10,
		-10, 0, 0, 0, 0, 0, 0, -10,
		-20, -10, -10, -10, -10, -10, -10, -20
	},
	// rook
	{
		0, 0, 0, 5, 5, 0, 0, 0,
		-5, 0, 0, 0, 0, 0, 0, -5,
		-5, 0, 0, 0, 0, 0, 0, -5,
		-5, 0, 0, 0, 0, 0, 0, -5,
		-5, 0, 0, 0, 0, 0, 0, -5,
		-5, 0, 0, 0, 0, 0, 0, -5,
		5, 10, 10, 10, 10, 10, 10, 5,
		0, 0, 0, 0, 0, 0, 0, 0
	},
	// king
	{
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-20, -30, -30, -40, -40, -30, -30, -20,
		-10, -20, -20, -20, -20, -20, -20, -10,
		20, 20, 0, 0, 0, 0, 20, 20,
		20, 30, 10, 0, 0, 10, 30, 20
	},
	// queen
	{
		-20, -10, -10, -5, -5, -10, -10, -20,
		-10, 0, 0, 0, 0, 0, 0, -10,
		-10, 0, 5, 5, 5, 5, 0, -10,
		-5, 0, 5, 5, 5, 5, 0, -5,
		0, 0, 5, 5, 5, 5, 0, -5,
		-10, 5, 5, 5, 5, 5, 0, -10,
		-10, 0, 5, 0, 0, 0, 0, -10,
		-20, -10, -10, -5, -5, -10, -10, -20
	}
};

struct EvalInfo {
	int whiteBishops = 0;
	int blackBishops = 0;
	int whitePawns[8] = {};
	int blackPawns[8] = {};
	int whiteKingRow = 0;
	int whiteKingCol = 0;
	int blackKingRow = 7;
	int blackKingCol = 7;
	int nonPawnMaterial = 0;
};

static int pst_index_for_piece(int piece, int row, int col)
{
	return piece > 0 ? ((7 - row) * 8 + col) : (row * 8 + col);
}

static bool is_passed_pawn(const Board& board, int row, int col, bool white)
{
	const int enemyPawn = white ? -PAWN : PAWN;
	const int dir = white ? 1 : -1;

	for (int r = row + dir; r >= 0 && r < 8; r += dir) {
		for (int dc = -1; dc <= 1; ++dc) {
			const int c = col + dc;
			if (in_bounds(r, c) && board.squares[r][c] == enemyPawn)
				return false;
		}
	}

	return true;
}

static int passed_pawn_bonus(int row, bool white)
{
	const int advancement = white ? row : 7 - row;
	return 15 + advancement * 8;
}

static bool counts_as_mobile_square(const Board& board, int row, int col, bool white)
{
	const int target = board.squares[row][col];
	return target == 0 || is_opponent_piece(target, white);
}

static int knight_mobility(const Board& board, int row, int col, bool white)
{
	static const int kOffsets[8][2] = {
		{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
		{1, -2},  {1, 2},  {2, -1},  {2, 1}
	};

	int count = 0;
	for (const auto& offset : kOffsets) {
		const int r = row + offset[0];
		const int c = col + offset[1];
		if (in_bounds(r, c) && counts_as_mobile_square(board, r, c, white))
			++count;
	}
	return count;
}

static int sliding_mobility(const Board& board, int row, int col, bool white,
							const int dirs[][2], int dirCount)
{
	int count = 0;

	for (int i = 0; i < dirCount; ++i) {
		int r = row + dirs[i][0];
		int c = col + dirs[i][1];

		while (in_bounds(r, c)) {
			const int target = board.squares[r][c];
			if (target == 0) {
				++count;
			} else {
				if (is_opponent_piece(target, white)) ++count;
				break;
			}

			r += dirs[i][0];
			c += dirs[i][1];
		}
	}

	return count;
}

static int mobility_bonus(const Board& board, int type, int row, int col, bool white)
{
	static const int kBishopDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
	static const int kRookDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	static const int kQueenDirs[8][2] = {
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
		{-1, 0},  {1, 0},  {0, -1}, {0, 1}
	};

	switch (type) {
		case KNIGHT: return knight_mobility(board, row, col, white) * 3;
		case BISHOP: return sliding_mobility(board, row, col, white, kBishopDirs, 4) * 3;
		case ROOK:   return sliding_mobility(board, row, col, white, kRookDirs, 4) * 2;
		case QUEEN:  return sliding_mobility(board, row, col, white, kQueenDirs, 8);
		default:     return 0;
	}
}

static int pawn_structure_score(const int whitePawns[8], const int blackPawns[8])
{
	int score = 0;

	for (int file = 0; file < 8; ++file) {
		if (whitePawns[file] > 1) score -= (whitePawns[file] - 1) * 12;
		if (blackPawns[file] > 1) score += (blackPawns[file] - 1) * 12;

		const bool whiteHasNeighbor =
			(file > 0 && whitePawns[file - 1] > 0) ||
			(file < 7 && whitePawns[file + 1] > 0);
		const bool blackHasNeighbor =
			(file > 0 && blackPawns[file - 1] > 0) ||
			(file < 7 && blackPawns[file + 1] > 0);

		if (whitePawns[file] > 0 && !whiteHasNeighbor) score -= 10;
		if (blackPawns[file] > 0 && !blackHasNeighbor) score += 10;
	}

	return score;
}

static int center_distance(int row, int col)
{
	int best = 20;
	for (int centerRow = 3; centerRow <= 4; ++centerRow) {
		for (int centerCol = 3; centerCol <= 4; ++centerCol) {
			const int distance =
				abs_piece(row - centerRow) + abs_piece(col - centerCol);
			if (distance < best) best = distance;
		}
	}
	return best;
}

static int endgame_king_activity(int whiteKingRow, int whiteKingCol,
								 int blackKingRow, int blackKingCol,
								 int nonPawnMaterial)
{
	if (nonPawnMaterial > 2400) return 0;

	const int whiteBonus = (6 - center_distance(whiteKingRow, whiteKingCol)) * 15;
	const int blackBonus = (6 - center_distance(blackKingRow, blackKingCol)) * 15;
	return whiteBonus - blackBonus;
}

static int piece_score(const Board& board, int piece, int row, int col)
{
	const int type = abs_piece(piece);
	const int sign = piece > 0 ? 1 : -1;
	const int idx = pst_index_for_piece(piece, row, col);

	int score = kPieceValue[type];
	score += kPieceSquareTable[type][idx];
	score += mobility_bonus(board, type, row, col, piece > 0);

	if (type == PAWN && is_passed_pawn(board, row, col, piece > 0))
		score += passed_pawn_bonus(row, piece > 0);

	return score * sign;
}

static void record_piece(EvalInfo& info, int piece, int row, int col)
{
	const int type = abs_piece(piece);

	if (type != PAWN && type != KING)
		info.nonPawnMaterial += kPieceValue[type];

	if (type == BISHOP) {
		if (piece > 0) ++info.whiteBishops;
		else ++info.blackBishops;
	}

	if (type == PAWN) {
		if (piece > 0) ++info.whitePawns[col];
		else ++info.blackPawns[col];
	}

	if (type == KING) {
		if (piece > 0) {
			info.whiteKingRow = row;
			info.whiteKingCol = col;
		} else {
			info.blackKingRow = row;
			info.blackKingCol = col;
		}
	}
}

static int final_position_score(const EvalInfo& info)
{
	int score = 0;

	if (info.whiteBishops >= 2) score += 35;
	if (info.blackBishops >= 2) score -= 35;
	score += pawn_structure_score(info.whitePawns, info.blackPawns);
	score += endgame_king_activity(info.whiteKingRow, info.whiteKingCol,
								   info.blackKingRow, info.blackKingCol,
								   info.nonPawnMaterial);

	return score;
}

int evaluate(const Board& board)
{
	int score = 0;
	EvalInfo info;

	for (int row = 0; row < 8; ++row)
	{
		for (int col = 0; col < 8; ++col) {
			int piece = board.squares[row][col];
			if (piece == 0) continue;

			score += piece_score(board, piece, row, col);
			record_piece(info, piece, row, col);
		}
	}

	return score + final_position_score(info);
}
