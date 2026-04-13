#pragma once

#include <string>
#include <vector>

namespace rg {

struct GameAnalysis {
    int grundy_value{0};
    bool recoverable{false};  // first player wins / non-zero nim-sum in our reduction
    std::string verdict;      // "recoverable" or "critical"
};

/*
 * Impartial game view of a cascade: independent subgames per affected train modeled as Nim heaps
 * of size ceil(delay_i / chunk). Grundy = XOR of heap sizes (standard Nim).
 * Recoverable iff XOR != 0 (next player to move can force win in Nim — proxy for "controller" advantage).
 *
 * References:
 * - R. Sprague & P. Grundy (theory of impartial games); see e.g. Berlekamp, Conway, Guy,
 *   Winning Ways for your Mathematical Plays.
 * - https://cp-algorithms.com/game_theory/sprague-grundy-nim.html
 */
int mex(const std::vector<int>& reachable_grundies);

// Nim heap of size n has Grundy number n.
int nimGrundy(int heap_size);

int xorSum(const std::vector<int>& values);

GameAnalysis analyzeCascadeNim(const std::vector<int>& heap_sizes);

// Map delays (minutes) to heap sizes: max(1, delay / chunk_minutes).
std::vector<int> delaysToHeaps(const std::vector<int>& delays_minutes, int chunk_minutes = 15);

}  // namespace rg
