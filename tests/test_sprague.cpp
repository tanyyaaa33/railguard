#include "intervention/SpragueGrundy.h"

#include <cassert>
#include <vector>

int main() {
    assert(rg::xorSum({1, 2, 3}) == 0);
    rg::GameAnalysis a = rg::analyzeCascadeNim({1, 2, 3});
    assert(a.grundy_value == 0);
    assert(a.verdict == "critical");

    rg::GameAnalysis b = rg::analyzeCascadeNim({1, 2});
    assert(b.grundy_value != 0);
    assert(b.verdict == "recoverable");

    std::vector<int> reach{0, 1, 2};
    assert(rg::mex(reach) == 3);

    std::vector<int> heaps = rg::delaysToHeaps({0, 15, 16}, 15);
    assert(heaps.size() == 3u);
    assert(heaps[0] == 0);
    assert(heaps[1] == 1);
    assert(heaps[2] == 2);

    return 0;
}
