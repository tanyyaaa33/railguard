#pragma once

#include <string>
#include <utility>
#include <vector>

namespace rg {

struct Station {
    int id{-1};
    std::string name;
    std::string zone;
    double lat{0};
    double lon{0};
};

// Undirected multi-edge-capable adjacency: neighbor vertex, parallel edge index (unused = 0).
// O(V) vertices; edges stored as unique pairs for Tarjan (simple graph).
class NetworkGraph {
public:
    void clear();
    void setNumVertices(int n);
    void addUndirectedEdge(int u, int v);

    int numVertices() const { return static_cast<int>(adj_.size()); }
    const std::vector<std::vector<int>>& adjacency() const { return adj_; }
    std::vector<std::vector<int>>& adjacency() { return adj_; }

    std::vector<Station> stations;

private:
    std::vector<std::vector<int>> adj_;
};

// Load stations.csv + routes.csv into graph (undirected).
bool loadNetworkFromCsv(const std::string& dataDir, NetworkGraph& out, std::string& err);

}  // namespace rg
