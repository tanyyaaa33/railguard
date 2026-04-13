#include "graph/NetworkGraph.h"

#include "io/CSVParser.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace rg {

void NetworkGraph::clear() {
    adj_.clear();
    stations.clear();
}

void NetworkGraph::setNumVertices(int n) {
    adj_.assign(static_cast<size_t>(n), {});
}

void NetworkGraph::addUndirectedEdge(int u, int v) {
    if (u < 0 || v < 0 || u >= numVertices() || v >= numVertices() || u == v) return;
    adj_[static_cast<size_t>(u)].push_back(v);
    adj_[static_cast<size_t>(v)].push_back(u);
}

bool loadNetworkFromCsv(const std::string& dataDir, NetworkGraph& out, std::string& err) {
    out.clear();
    err.clear();

    std::string sp = dataDir;
    if (!sp.empty() && sp.back() != '/') sp += '/';

    {
        std::ifstream f(sp + "stations.csv");
        if (!f) {
            err = "cannot open stations.csv";
            return false;
        }
        std::string line;
        if (!std::getline(f, line)) {
            err = "empty stations.csv";
            return false;
        }
        std::vector<Station> sts;
        while (std::getline(f, line)) {
            auto row = parseCsvLine(line);
            if (row.size() < 5) continue;
            Station s;
            s.id = std::stoi(row[0]);
            s.name = row[1];
            s.zone = row[2];
            s.lat = std::stod(row[3]);
            s.lon = std::stod(row[4]);
            sts.push_back(s);
        }
        int maxId = -1;
        for (const auto& s : sts) maxId = std::max(maxId, s.id);
        out.setNumVertices(maxId + 1);
        out.stations.resize(static_cast<size_t>(maxId + 1));
        for (const auto& s : sts) {
            if (s.id >= 0 && s.id < out.numVertices()) out.stations[static_cast<size_t>(s.id)] = s;
        }
    }

    {
        std::ifstream f(sp + "routes.csv");
        if (!f) {
            err = "cannot open routes.csv";
            return false;
        }
        std::string line;
        if (!std::getline(f, line)) {
            err = "empty routes.csv";
            return false;
        }
        while (std::getline(f, line)) {
            auto row = parseCsvLine(line);
            if (row.size() < 4) continue;
            int u = std::stoi(row[0]);
            int v = std::stoi(row[1]);
            out.addUndirectedEdge(u, v);
        }
    }

    return true;
}

}  // namespace rg
