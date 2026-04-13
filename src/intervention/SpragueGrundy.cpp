/*
 * Nim reduction of cascade heaps; XOR grundy in O(n); mex helper for extensions.
 * https://cp-algorithms.com/game_theory/sprague-grundy-nim.html
 */
#include "intervention/SpragueGrundy.h"

#include <algorithm>
#include <unordered_set>

namespace rg {

int mex(const std::vector<int>& reachable_grundies) {
    std::unordered_set<int> s(reachable_grundies.begin(), reachable_grundies.end());
    int g = 0;
    while (s.count(g)) ++g;
    return g;
}

int nimGrundy(int heap_size) { return std::max(0, heap_size); }

int xorSum(const std::vector<int>& values) {
    int x = 0;
    for (int v : values) x ^= v;
    return x;
}

GameAnalysis analyzeCascadeNim(const std::vector<int>& heap_sizes) {
    GameAnalysis a;
    std::vector<int> gs;
    gs.reserve(heap_sizes.size());
    for (int h : heap_sizes) gs.push_back(nimGrundy(h));
    a.grundy_value = xorSum(gs);
    a.recoverable = (a.grundy_value != 0);
    a.verdict = a.recoverable ? "recoverable" : "critical";
    return a;
}

std::vector<int> delaysToHeaps(const std::vector<int>& delays_minutes, int chunk_minutes) {
    std::vector<int> out;
    out.reserve(delays_minutes.size());
    for (int d : delays_minutes) {
        int c = std::max(1, chunk_minutes);
        int h = d <= 0 ? 0 : (d + c - 1) / c;
        out.push_back(h);
    }
    return out;
}

}  // namespace rg
