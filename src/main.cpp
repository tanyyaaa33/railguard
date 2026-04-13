#include "analytics/PassengerImpact.h"
#include "analytics/SlidingWindow.h"
#include "cascade/DelayDAG.h"
#include "graph/NetworkGraph.h"
#include "graph/TarjanBridge.h"
#include "intervention/InterventionOptimizer.h"
#include "intervention/SpragueGrundy.h"

#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::string dataDir() {
    const char* e = std::getenv("RAILGUARD_DATA");
    if (e && e[0]) return std::string(e);
    return "data";
}

void usage() {
    std::cerr
        << "railguard — Indian Railways cascade & network analytics\n"
        << "Usage:\n"
        << "  railguard --analyze-network\n"
        << "  railguard --cascade <train_id> <delay_minutes>\n"
        << "  railguard --hotspots --window <days> [--threshold <T>]\n"
        << "  railguard --intervene <train_id> --budget <K> [--seed-delay <m>] [--pdm-threshold <T>]\n"
        << "  railguard --game-state <cascade_id>   (cascade_id = trainId_delayMin, e.g. 5_40)\n";
}

bool parseInt(const char* s, int& out) {
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!s || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

bool parseCascadeId(const std::string& s, int& train_id, int& delay0) {
    auto p = s.find('_');
    if (p == std::string::npos) return false;
    return parseInt(s.substr(0, p).c_str(), train_id) && parseInt(s.substr(p + 1).c_str(), delay0);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    const std::string dd = dataDir();
    std::string err;

    std::string cmd = argv[1];
    if (cmd == "--analyze-network") {
        rg::NetworkGraph g;
        if (!rg::loadNetworkFromCsv(dd, g, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        rg::TarjanResult t = rg::runTarjan(g);
        std::cout << "Bridges (u,v), u<v, |E_bridge|=" << t.bridges.size() << "\n";
        for (const auto& e : t.bridges) std::cout << "  " << e.first << " " << e.second << "\n";
        std::cout << "Articulation points: ";
        for (size_t i = 0; i < t.articulation_points.size(); ++i) {
            if (i) std::cout << ", ";
            int vid = t.articulation_points[i];
            std::cout << vid;
            if (vid >= 0 && vid < static_cast<int>(g.stations.size())) std::cout << " (" << g.stations[static_cast<size_t>(vid)].name << ")";
        }
        std::cout << "\n";
        return 0;
    }

    if (cmd == "--cascade") {
        if (argc < 4) {
            usage();
            return 1;
        }
        int tid, dm;
        if (!parseInt(argv[2], tid) || !parseInt(argv[3], dm)) {
            std::cerr << "bad train_id or delay_minutes\n";
            return 1;
        }
        rg::DelayDAG dag;
        if (!dag.loadTrainsCsv(dd, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        rg::CascadeResult r = dag.propagateFromTrainDelay(tid, dm);
        if (!r.ok) {
            std::cerr << r.error << "\n";
            return 1;
        }
        std::cout << "Affected trains (train_id, propagated_delay_min):\n";
        for (const auto& pr : r.affected_trains) std::cout << "  " << pr.first << " " << pr.second << "\n";
        return 0;
    }

    if (cmd == "--hotspots") {
        int W = 7;
        double thr = 35.0;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--window" && i + 1 < argc) {
                if (!parseInt(argv[++i], W)) {
                    std::cerr << "bad --window\n";
                    return 1;
                }
            } else if (a == "--threshold" && i + 1 < argc) {
                thr = std::strtod(argv[++i], nullptr);
            }
        }
        rg::SlidingWindowAnalyzer sw;
        if (!sw.load(dd, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        auto hs = sw.hotspotsFixed(W, thr, 10);
        sw.fillVariableWindowStats(hs);
        std::cout << "Top " << hs.size() << " stations by " << W << "-day mean daily delay (avg over trains that day)\n";
        std::cout << "threshold T=" << thr << " (flagged if mean > T)\n";
        for (const auto& h : hs) {
            std::cout << h.station_id << " " << h.name << " fixed_mean=" << h.fixed_window_mean
                      << " var_max_mean=" << h.variable_window_mean << " var_best_len=" << h.variable_best_len
                      << (h.fixed_window_mean > thr ? " HOT" : "") << "\n";
        }
        return 0;
    }

    if (cmd == "--intervene") {
        if (argc < 4) {
            usage();
            return 1;
        }
        int seed_tid = -1;
        int budget = 1;
        int seed_delay = 60;
        long long pdm_thr = LLONG_MAX;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--budget" && i + 1 < argc) {
                if (!parseInt(argv[++i], budget)) {
                    std::cerr << "bad --budget\n";
                    return 1;
                }
            } else if (a == "--seed-delay" && i + 1 < argc) {
                if (!parseInt(argv[++i], seed_delay)) {
                    std::cerr << "bad --seed-delay\n";
                    return 1;
                }
            } else if (a == "--pdm-threshold" && i + 1 < argc) {
                pdm_thr = std::strtoll(argv[++i], nullptr, 10);
            } else if (seed_tid < 0) {
                if (!parseInt(argv[i], seed_tid)) {
                    std::cerr << "bad train_id\n";
                    return 1;
                }
            }
        }
        if (seed_tid < 0) {
            usage();
            return 1;
        }
        rg::DelayDAG dag;
        if (!dag.loadTrainsCsv(dd, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        rg::CascadeResult r = dag.propagateFromTrainDelay(seed_tid, seed_delay);
        if (!r.ok) {
            std::cerr << r.error << "\n";
            return 1;
        }
        rg::PassengerModel pax;
        if (!pax.loadTrainsCsv(dd, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        rg::InterventionPlan plan = rg::optimizeInterventionsGreedy(r.affected_trains, pax, budget);
        std::cout << "Cascade from train " << seed_tid << " (" << seed_delay << " min seed): PDM before="
                  << plan.passenger_delay_before
                  << " after " << budget << " greedy clears=" << plan.passenger_delay_after << "\n";
        std::cout << "Chosen trains:";
        for (int t : plan.chosen_trains) std::cout << " " << t;
        std::cout << "\n";
        if (pdm_thr != LLONG_MAX) {
            int mk = rg::minInterventionsForThreshold(r.affected_trains, pax, budget, pdm_thr);
            std::cout << "Min k (<=budget) greedy to reach PDM<=" << pdm_thr << ": " << mk << "\n";
        }
        return 0;
    }

    if (cmd == "--game-state") {
        if (argc < 3) {
            usage();
            return 1;
        }
        std::string cid = argv[2];
        int seed_tid, d0;
        if (!parseCascadeId(cid, seed_tid, d0)) {
            std::cerr << "bad cascade_id (use trainId_delayMin)\n";
            return 1;
        }
        rg::DelayDAG dag;
        if (!dag.loadTrainsCsv(dd, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        rg::CascadeResult r = dag.propagateFromTrainDelay(seed_tid, d0);
        if (!r.ok) {
            std::cerr << r.error << "\n";
            return 1;
        }
        std::vector<int> delays;
        delays.reserve(r.affected_trains.size());
        for (const auto& pr : r.affected_trains) delays.push_back(pr.second);
        auto heaps = rg::delaysToHeaps(delays, 15);
        rg::GameAnalysis g = rg::analyzeCascadeNim(heaps);
        std::cout << "cascade_id=" << cid << " heaps=";
        for (size_t i = 0; i < heaps.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << heaps[i];
        }
        std::cout << "\nGrundy XOR=" << g.grundy_value << " verdict=" << g.verdict << "\n";
        return 0;
    }

    usage();
    return 1;
}
