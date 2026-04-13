#pragma once

#include "graph/NetworkGraph.h"

#include <utility>
#include <vector>

namespace rg {

struct TarjanResult {
    // Bridge edges as unordered pairs (min,max) for stable output.
    std::vector<std::pair<int, int>> bridges;
    std::vector<int> articulation_points;  // sorted unique vertex ids
};

/*
 * Bridges & articulation points in O(V+E) — undirected simple graph.
 *
 * References:
 * - R. Tarjan, "A Note on Finding the Bridges of a Graph", Inf. Proc. Letters 1974.
 * - T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein,
 *   Introduction to Algorithms, 3rd ed., §22.3 (DFS, discovery/finish times; bridge ideas).
 * - https://cp-algorithms.com/graph/bridge-searching.html
 * - https://cp-algorithms.com/graph/cutpoints.html
 *
 * Disconnected graphs: DFS restarted from every unvisited vertex — O(V+E) total.
 */
TarjanResult runTarjan(const NetworkGraph& g);

}  // namespace rg
