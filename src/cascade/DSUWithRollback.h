#pragma once

#include <utility>
#include <vector>

namespace rg {

/*
 * Disjoint Set Union with undo (rollback last k unions).
 * - Union by rank; **no path compression** (required so history stays valid).
 * - find(x): O(tree height) = O(log n) amortized with union-by-rank.
 * - rollback(k): undo last k successful unions in O(k) (stack pops + parent/rank restore); find is O(log n) without compression.
 *
 * References:
 * - Cormen et al., CLRS 3rd ed., §21.3 (Disjoint-set forests; union by rank).
 * - https://cp-algorithms.com/data_structures/disjoint_set_union.html
 * - Competitive Programming trick: "DSU rollback" (stack of modifications).
 */
class DSUWithRollback {
public:
    explicit DSUWithRollback(int n = 0);

    void reset(int n);

    // No path compression — walk parents until root.
    int find(int x);

    // Returns true if merged; false if already same set (nothing pushed).
    bool unite(int a, int b);

    // Undo the last k successful unions (only those that returned true count).
    void rollback(size_t k);

    size_t stackDepth() const { return history_.size(); }

private:
    struct Op {
        int child_root;   // vertex whose parent was changed from self to parent
        int new_parent;
        bool inc_rank;    // whether rank[new_parent] was incremented
        int rank_before;  // rank of new_parent before possible increment
    };

    std::vector<int> parent_;
    std::vector<int> rank_;
    std::vector<Op> history_;
};

}  // namespace rg
