#pragma once

#include "chess/types.h"

#include <cstdint>

constexpr int kInfinity  = 100000000;
constexpr int kMateScore = kInfinity - 1000;

enum class TTFlag : std::uint8_t {
    Exact,
    LowerBound,
    UpperBound,
};

struct TTEntry {
    std::uint64_t hash = 0;
    int depth = -1;
    int score = 0;
    TTFlag flag = TTFlag::Exact;
    Move bestMove{0, 0, 0, 0, 0, false, false};
    bool valid = false;
};

TTEntry* probe_tt(std::uint64_t hash);
void     store_tt(std::uint64_t hash, int depth, int score, TTFlag flag, const Move& bestMove);
int      score_to_tt(int score, int ply);
int      score_from_tt(int score, int ply);
