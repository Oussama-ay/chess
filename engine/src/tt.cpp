#include "chess/tt.h"

#include <cstddef>
#include <cstdint>
#include <vector>

static constexpr std::size_t kTTSize = 1u << 18;
static std::vector<TTEntry> g_tt(kTTSize);

TTEntry* probe_tt(std::uint64_t hash) {
    TTEntry& e = g_tt[hash & (kTTSize - 1)];
    return (e.valid && e.hash == hash) ? &e : nullptr;
}

void store_tt(std::uint64_t hash, int depth, int score, TTFlag flag, const Move& bestMove) {
    TTEntry& e = g_tt[hash & (kTTSize - 1)];
    if (!e.valid || depth >= e.depth || e.hash != hash) {
        e = {hash, depth, score, flag, bestMove, true};
    }
}

int score_to_tt(int score, int ply) {
    if (score >= kMateScore - 1000) return score + ply;
    if (score <= -kMateScore + 1000) return score - ply;
    return score;
}

int score_from_tt(int score, int ply) {
    if (score >= kMateScore - 1000) return score - ply;
    if (score <= -kMateScore + 1000) return score + ply;
    return score;
}
