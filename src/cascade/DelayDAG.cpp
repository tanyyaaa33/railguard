/*
 * Delay DAG: Kahn topological sort + weighted longest-path delay relaxation.
 *
 * Edge weights model two physical phenomena:
 *
 *   Along-route edge (ti,h) -> (ti,h+1), weight = 0:
 *     A train that is late at station h arrives late at h+1 by the same amount.
 *     No time can be recovered while moving; delay propagates unchanged.
 *
 *   Platform-conflict edge (A at S) -> (B at S), weight = turnaround(S) - gap(A,B):
 *     When A departs, the platform needs `turnaround` minutes before B can use it.
 *     If the scheduled gap between A and B is already >= turnaround, the schedule
 *     provides a buffer and negative weight absorbs part of A's delay.
 *     If the gap < turnaround (tight schedule), positive weight compounds the delay.
 *
 *     Physical formula:
 *       B_delay = max(0, A_delay + turnaround(S) - gap(A,B))
 *     which equals max(0, A_delay + weight).
 *
 * Propagation is applied in Kahn topological order so every predecessor is
 * finalised before we relax its outgoing edges — standard DAG longest path.
 *   // O(V+E) — Tarjan 1972 / Kahn 1962 / CLRS §22.4
 *
 * References:
 * - T. H. Cormen et al., Introduction to Algorithms, 3rd ed., §22.4.
 * - https://cp-algorithms.com/graph/topological-sort.html
 */

#include "cascade/DelayDAG.h"

#include "io/CSVParser.h"

#include <algorithm>
#include <fstream>
#include <queue>
#include <unordered_map>

namespace rg {

namespace {

/*
 * Platform turnaround time (minutes) per station.
 * Major junctions (higher station_id variance) get longer turnarounds.
 * Range: 15–25 min, deterministically varied by station id.
 */
int platformTurnaround(int station_id) {
    return 15 + (station_id % 11);  // [15, 25]
}

}  // namespace

bool DelayDAG::loadTrainsCsv(const std::string& dataDir, std::string& err) {
    trains_.clear();
    err.clear();
    std::string sp = dataDir;
    if (!sp.empty() && sp.back() != '/') sp += '/';
    std::ifstream f(sp + "trains.csv");
    if (!f) {
        err = "cannot open trains.csv";
        return false;
    }
    std::string line;
    if (!std::getline(f, line)) {
        err = "empty trains.csv";
        return false;
    }
    while (std::getline(f, line)) {
        auto row = parseCsvLine(line);
        if (row.size() < 4) continue;
        TrainRoute tr;
        tr.train_id = std::stoi(row[0]);
        tr.name = row[1];
        const std::string& rpart = row[2];
        const std::string& spart = row[3];
        auto splitInts = [](const std::string& s, std::vector<int>& out) {
            size_t p = 0;
            while (p < s.size()) {
                size_t q = s.find(';', p);
                std::string tok = (q == std::string::npos) ? s.substr(p) : s.substr(p, q - p);
                if (!tok.empty()) out.push_back(std::stoi(tok));
                if (q == std::string::npos) break;
                p = q + 1;
            }
        };
        splitInts(rpart, tr.stations);
        splitInts(spart, tr.schedule_mins);
        while (tr.schedule_mins.size() < tr.stations.size())
            tr.schedule_mins.push_back(tr.schedule_mins.empty() ? 0 : tr.schedule_mins.back());
        trains_.push_back(std::move(tr));
    }
    return !trains_.empty();
}

void DelayDAG::buildGraph(std::vector<std::vector<std::pair<int, int>>>& adj,
                          std::vector<int>& indeg,
                          std::vector<std::pair<int, int>>& node_train_hop,
                          std::vector<int>& node_offsets,
                          int& n_nodes) const {
    // Precompute per-train node id offsets: O(T).
    node_offsets.resize(trains_.size() + 1, 0);
    for (size_t i = 0; i < trains_.size(); ++i)
        node_offsets[i + 1] = node_offsets[i] + static_cast<int>(trains_[i].stations.size());
    n_nodes = node_offsets[trains_.size()];

    auto nodeId = [&](int ti, int h) { return node_offsets[static_cast<size_t>(ti)] + h; };

    // Build node_train_hop lookup.
    node_train_hop.clear();
    node_train_hop.reserve(static_cast<size_t>(n_nodes));
    for (size_t ti = 0; ti < trains_.size(); ++ti)
        for (size_t h = 0; h < trains_[ti].stations.size(); ++h)
            node_train_hop.emplace_back(static_cast<int>(ti), static_cast<int>(h));

    adj.assign(static_cast<size_t>(n_nodes), {});
    indeg.assign(static_cast<size_t>(n_nodes), 0);

    // ── Along-route edges: weight = 0 ─────────────────────────────────────────
    // Delay propagates unchanged hop-to-hop; train cannot recover time en route.
    for (size_t ti = 0; ti < trains_.size(); ++ti) {
        int nhops = static_cast<int>(trains_[ti].stations.size());
        for (int h = 0; h + 1 < nhops; ++h) {
            int u = nodeId(static_cast<int>(ti), h);
            int v = nodeId(static_cast<int>(ti), h + 1);
            adj[static_cast<size_t>(u)].emplace_back(v, 0);
            indeg[static_cast<size_t>(v)]++;
        }
    }

    // ── Platform-conflict edges: weight = turnaround(S) - schedule_gap ────────
    // Group all (train_idx, hop) visits by station id, then sort by schedule.
    std::unordered_map<int, std::vector<std::pair<int, int>>> at_station;
    for (size_t ti = 0; ti < trains_.size(); ++ti)
        for (size_t h = 0; h < trains_[ti].stations.size(); ++h)
            at_station[trains_[ti].stations[h]].emplace_back(static_cast<int>(ti), static_cast<int>(h));

    for (auto& kv : at_station) {
        const int sid = kv.first;
        auto& vec = kv.second;
        if (vec.size() < 2) continue;

        std::sort(vec.begin(), vec.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            int ta = trains_[static_cast<size_t>(a.first)].schedule_mins[static_cast<size_t>(a.second)];
            int tb = trains_[static_cast<size_t>(b.first)].schedule_mins[static_cast<size_t>(b.second)];
            if (ta != tb) return ta < tb;
            return a.first < b.first;
        });

        const int turnaround = platformTurnaround(sid);

        for (size_t i = 0; i + 1 < vec.size(); ++i) {
            int ti_a = vec[i].first,     h_a = vec[i].second;
            int ti_b = vec[i + 1].first, h_b = vec[i + 1].second;

            int sched_a = trains_[static_cast<size_t>(ti_a)].schedule_mins[static_cast<size_t>(h_a)];
            int sched_b = trains_[static_cast<size_t>(ti_b)].schedule_mins[static_cast<size_t>(h_b)];
            int gap = sched_b - sched_a;  // >= 0 by sort order

            // weight = turnaround - gap
            //   > 0  => tight schedule; B's delay = A's delay + extra (compounding)
            //   < 0  => wide buffer;    B's delay = A's delay - absorbed (decay)
            //   = 0  => exactly meets turnaround; delay passes through unchanged
            int weight = turnaround - gap;

            int u = nodeId(ti_a, h_a);
            int v = nodeId(ti_b, h_b);
            adj[static_cast<size_t>(u)].emplace_back(v, weight);
            indeg[static_cast<size_t>(v)]++;
        }
    }
}

CascadeResult DelayDAG::propagateFromTrainDelay(int train_id, int initial_delay_minutes) {
    CascadeResult res;
    int ti = -1;
    for (size_t i = 0; i < trains_.size(); ++i) {
        if (trains_[i].train_id == train_id) {
            ti = static_cast<int>(i);
            break;
        }
    }
    if (ti < 0) {
        res.ok = false;
        res.error = "train_id not found";
        return res;
    }

    std::vector<std::vector<std::pair<int, int>>> adj;
    std::vector<int> indeg;
    std::vector<std::pair<int, int>> node_train_hop;
    std::vector<int> node_offsets;
    int n = 0;
    buildGraph(adj, indeg, node_train_hop, node_offsets, n);

    // Seed the originating train at its first hop.
    const int seed = node_offsets[static_cast<size_t>(ti)];
    std::vector<int> delay(static_cast<size_t>(n), 0);
    delay[static_cast<size_t>(seed)] = initial_delay_minutes;

    // ── Kahn topological sort — O(V+E) ────────────────────────────────────────
    std::queue<int> q;
    std::vector<int> cnt = indeg;
    for (int i = 0; i < n; ++i)
        if (cnt[static_cast<size_t>(i)] == 0) q.push(i);

    std::vector<int> order;
    order.reserve(static_cast<size_t>(n));
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        order.push_back(u);
        for (const auto& e : adj[static_cast<size_t>(u)]) {
            if (--cnt[static_cast<size_t>(e.first)] == 0) q.push(e.first);
        }
    }

    if (static_cast<int>(order.size()) != n) {
        res.ok = false;
        res.error = "cycle detected in delay DAG";
        return res;
    }

    // ── Weighted delay relaxation in topological order — O(V+E) ───────────────
    // Physical rule:
    //   Along-route (weight=0):  delay[v] = max(delay[v], delay[u])
    //   Platform    (weight=w):  delay[v] = max(delay[v], max(0, delay[u] + w))
    //
    // Only propagate when the upstream node actually carries delay; this prevents
    // platform turnaround baseline from inflating unaffected trains.
    for (int u : order) {
        const int du = delay[static_cast<size_t>(u)];
        if (du <= 0) continue;  // no delay to propagate from this node
        for (const auto& e : adj[static_cast<size_t>(u)]) {
            const int v = e.first, w = e.second;
            const int prop = du + w;          // may be negative when buffer > delay
            if (prop > 0 && prop > delay[static_cast<size_t>(v)])
                delay[static_cast<size_t>(v)] = prop;
        }
    }

    res.topo_order = std::move(order);
    res.node_delay = std::move(delay);

    // Collect maximum propagated delay per train_id (across all its hops).
    std::unordered_map<int, int> best;
    for (int nid = 0; nid < n; ++nid) {
        const int d = res.node_delay[static_cast<size_t>(nid)];
        if (d <= 0) continue;
        const int tix = node_train_hop[static_cast<size_t>(nid)].first;
        const int tid = trains_[static_cast<size_t>(tix)].train_id;
        auto it = best.find(tid);
        if (it == best.end() || d > it->second) best[tid] = d;
    }
    for (const auto& kv : best) res.affected_trains.emplace_back(kv);
    std::sort(res.affected_trains.begin(), res.affected_trains.end());

    return res;
}

}  // namespace rg
