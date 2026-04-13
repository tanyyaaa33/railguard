#include "cascade/DSUWithRollback.h"

#include <utility>

namespace rg {

DSUWithRollback::DSUWithRollback(int n) { reset(n); }

void DSUWithRollback::reset(int n) {
    history_.clear();
    parent_.resize(static_cast<size_t>(n));
    rank_.assign(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i) parent_[static_cast<size_t>(i)] = i;
}

int DSUWithRollback::find(int x) {
    int cur = x;
    while (parent_[static_cast<size_t>(cur)] != cur) cur = parent_[static_cast<size_t>(cur)];
    return cur;
}

bool DSUWithRollback::unite(int a, int b) {
    int ra = find(a);
    int rb = find(b);
    if (ra == rb) return false;

    // Attach smaller rank under larger rank (swap so ra is deeper root).
    if (rank_[static_cast<size_t>(ra)] < rank_[static_cast<size_t>(rb)]) std::swap(ra, rb);

    Op op;
    op.child_root = rb;
    op.new_parent = ra;
    op.inc_rank = false;
    op.rank_before = rank_[static_cast<size_t>(ra)];

    parent_[static_cast<size_t>(rb)] = ra;
    if (rank_[static_cast<size_t>(ra)] == rank_[static_cast<size_t>(rb)]) {
        rank_[static_cast<size_t>(ra)]++;
        op.inc_rank = true;
    }
    history_.push_back(op);
    return true;
}

void DSUWithRollback::rollback(size_t k) {
    while (k > 0 && !history_.empty()) {
        Op op = history_.back();
        history_.pop_back();
        rank_[static_cast<size_t>(op.new_parent)] = op.rank_before;
        parent_[static_cast<size_t>(op.child_root)] = op.child_root;
        --k;
    }
}

}  // namespace rg
