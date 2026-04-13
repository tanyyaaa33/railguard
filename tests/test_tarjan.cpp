#include "graph/NetworkGraph.h"
#include "graph/TarjanBridge.h"

#include <cassert>
#include <utility>

// Hand-built path P4: bridges are all edges; articulation points are internal vertices.
// O(V+E) — Tarjan 1972 / CLRS §22.3 / cp-algorithms bridge-searching.

int main() {
    rg::NetworkGraph g;
    g.setNumVertices(4);
    g.addUndirectedEdge(0, 1);
    g.addUndirectedEdge(1, 2);
    g.addUndirectedEdge(2, 3);

    rg::TarjanResult r = rg::runTarjan(g);
    assert(r.bridges.size() == 3u);
    assert(r.bridges[0] == std::make_pair(0, 1));
    assert(r.bridges[1] == std::make_pair(1, 2));
    assert(r.bridges[2] == std::make_pair(2, 3));

    assert(r.articulation_points.size() == 2u);
    assert(r.articulation_points[0] == 1);
    assert(r.articulation_points[1] == 2);

    // Disconnected: add isolated vertex by expanding to 5 nodes, only path on 0..3
    rg::NetworkGraph h;
    h.setNumVertices(5);
    h.addUndirectedEdge(0, 1);
    h.addUndirectedEdge(1, 2);
    h.addUndirectedEdge(2, 3);
    rg::TarjanResult r2 = rg::runTarjan(h);
    assert(r2.bridges.size() == 3u);
    assert(r2.articulation_points.size() == 2u);

    return 0;
}
