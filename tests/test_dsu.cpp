#include "cascade/DSUWithRollback.h"

#include <cassert>

// 5 successful unions, rollback(2), assert component structure — CLRS §21.3 DSU forests.

int main() {
    rg::DSUWithRollback d(6);
    assert(d.unite(0, 1));
    assert(d.unite(2, 3));
    assert(d.unite(4, 5));
    assert(d.unite(1, 2));
    assert(d.unite(3, 4));
    assert(d.find(0) == d.find(5));

    d.rollback(2);

    assert(d.find(0) == d.find(1));
    assert(d.find(2) == d.find(3));
    assert(d.find(4) == d.find(5));
    assert(d.find(0) != d.find(2));
    assert(d.find(2) != d.find(4));

    return 0;
}
