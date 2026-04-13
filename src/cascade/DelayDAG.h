#pragma once

#include <string>
#include <utility>
#include <vector>

namespace rg {

struct TrainRoute {
    int train_id{-1};
    std::string name;
    std::vector<int> stations;       // station ids along route
    std::vector<int> schedule_mins;  // per-hop scheduled minute-of-day
};

struct CascadeResult {
    bool ok{true};
    std::string error;
    std::vector<int> topo_order;  // node indices in topological order
    std::vector<int> node_delay;  // propagated delay minutes per node
    std::vector<std::pair<int, int>> affected_trains;  // train_id, final delay
};

/*
 * Delay precedence DAG: node = (train, hop index at a station).
 * Edges:
 *   (1) Along-route: (ti,h) -> (ti,h+1), weight = 0.
 *       A delayed train cannot recover time en route; delay propagates unchanged.
 *   (2) Same-station platform queue, sorted by scheduled minute:
 *       (A at S) -> (B at S), weight = platformTurnaround(S) - schedule_gap(A,B).
 *       Positive weight  => gap < turnaround  => B gets compounded delay.
 *       Negative weight  => gap > turnaround  => buffer absorbs part of A's delay.
 *
 * Propagation rule (applied in Kahn topological order — O(V+E)):
 *   if delay[u] > 0:  delay[v] = max(delay[v], max(0, delay[u] + edge_weight))
 *
 * References:
 * - T. H. Cormen et al., CLRS 3rd ed., §22.4 (Topological sort).
 * - https://cp-algorithms.com/graph/topological-sort.html
 */
class DelayDAG {
public:
    bool loadTrainsCsv(const std::string& dataDir, std::string& err);

    // Seed delay at the train's route origin (hop 0); propagate through DAG.
    CascadeResult propagateFromTrainDelay(int train_id, int initial_delay_minutes);

    const std::vector<TrainRoute>& trains() const { return trains_; }

private:
    std::vector<TrainRoute> trains_;

    // adj stores (neighbor_node_id, edge_weight_minutes) pairs.
    void buildGraph(std::vector<std::vector<std::pair<int, int>>>& adj,
                    std::vector<int>& indeg,
                    std::vector<std::pair<int, int>>& node_train_hop,
                    std::vector<int>& node_offsets,
                    int& n_nodes) const;
};

}  // namespace rg
