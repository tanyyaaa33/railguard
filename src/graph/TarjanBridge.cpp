#include "graph/TarjanBridge.h"

#include <algorithm>
#include <set>

namespace rg {

namespace {

struct State {
    int n{0};
    const std::vector<std::vector<int>>* adj{nullptr};
    std::vector<int> tin;
    std::vector<int> low;
    std::vector<char> vis;
    int timer{0};
    std::vector<std::pair<int, int>> bridges;
    std::set<int> cuts;

    void dfs(int v, int p) {
        vis[static_cast<size_t>(v)] = 1;
        tin[static_cast<size_t>(v)] = low[static_cast<size_t>(v)] = timer++;
        int children = 0;
        for (int to : (*adj)[static_cast<size_t>(v)]) {
            if (to == p) continue;
            if (vis[static_cast<size_t>(to)]) {
                low[static_cast<size_t>(v)] =
                    std::min(low[static_cast<size_t>(v)], tin[static_cast<size_t>(to)]);
            } else {
                dfs(to, v);
                low[static_cast<size_t>(v)] =
                    std::min(low[static_cast<size_t>(v)], low[static_cast<size_t>(to)]);
                if (low[static_cast<size_t>(to)] > tin[static_cast<size_t>(v)]) {
                    int a = v, b = to;
                    if (a > b) std::swap(a, b);
                    bridges.emplace_back(a, b);
                }
                if (low[static_cast<size_t>(to)] >= tin[static_cast<size_t>(v)] && p != -1) {
                    cuts.insert(v);
                }
                ++children;
            }
        }
        if (p == -1 && children > 1) cuts.insert(v);
    }
};

}  // namespace

TarjanResult runTarjan(const NetworkGraph& g) {
    TarjanResult res;
    const int n = g.numVertices();
    if (n <= 0) return res;

    State st;
    st.n = n;
    st.adj = &g.adjacency();
    st.tin.assign(static_cast<size_t>(n), -1);
    st.low.assign(static_cast<size_t>(n), -1);
    st.vis.assign(static_cast<size_t>(n), 0);

    for (int i = 0; i < n; ++i) {
        if (!st.vis[static_cast<size_t>(i)]) st.dfs(i, -1);
    }

    res.bridges = std::move(st.bridges);
    std::sort(res.bridges.begin(), res.bridges.end());
    res.bridges.erase(std::unique(res.bridges.begin(), res.bridges.end()), res.bridges.end());

    res.articulation_points.assign(st.cuts.begin(), st.cuts.end());
    return res;
}

}  // namespace rg
