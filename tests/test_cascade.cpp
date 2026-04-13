#include "cascade/DelayDAG.h"

#include <cassert>
#include <cstdlib>
#include <string>

static std::string dataDir() {
    const char* e = std::getenv("RAILGUARD_DATA");
    if (e && e[0]) return std::string(e);
    return "../data";
}

int main() {
    rg::DelayDAG dag;
    std::string err;
    if (!dag.loadTrainsCsv(dataDir(), err)) {
        if (!dag.loadTrainsCsv("data", err)) return 2;
    }

    const int SEED = 25;
    rg::CascadeResult r = dag.propagateFromTrainDelay(0, SEED);
    assert(r.ok);
    assert(!r.affected_trains.empty());

    // Every reported train must have a positive delay.
    for (const auto& pr : r.affected_trains) {
        assert(pr.first >= 0);
        assert(pr.second > 0);
    }

    // The seed train (id=0) must carry at least the original seed delay.
    bool found_seed = false;
    int max_delay = 0;
    int min_delay = INT_MAX;
    for (const auto& pr : r.affected_trains) {
        if (pr.first == 0) {
            assert(pr.second >= SEED);
            found_seed = true;
        }
        if (pr.second > max_delay) max_delay = pr.second;
        if (pr.second < min_delay) min_delay = pr.second;
    }
    assert(found_seed);

    // Compounding: at least one train must exceed the seed delay.
    assert(max_delay > SEED);

    // Variation: max and min must differ (not flat propagation).
    assert(max_delay > min_delay);

    return 0;
}
