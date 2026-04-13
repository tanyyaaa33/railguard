/*
 * Greedy intervention selection O(T log T); binary search on k with greedy prefix O(log K * T log T).
 */
#include "intervention/InterventionOptimizer.h"

#include "analytics/PassengerImpact.h"

#include <algorithm>

namespace rg {

namespace {

long long pdmOf(const std::vector<std::pair<int, int>>& v, const PassengerModel& pax) {
    long long s = 0;
    for (const auto& pr : v) s += pax.passengerDelayMinutes(pr.first, pr.second);
    return s;
}

}  // namespace

InterventionPlan optimizeInterventionsGreedy(const std::vector<std::pair<int, int>>& affected_train_delay,
                                             const PassengerModel& pax, int budget_k) {
    InterventionPlan plan;
    std::vector<std::pair<int, int>> cur = affected_train_delay;
    plan.passenger_delay_before = pdmOf(cur, pax);
    if (budget_k <= 0 || cur.empty()) {
        plan.passenger_delay_after = plan.passenger_delay_before;
        return plan;
    }

    struct Cand {
        int train{-1};
        long long gain{0};
    };
    std::vector<Cand> cands;
    cands.reserve(cur.size());
    for (const auto& pr : cur) {
        if (pr.second <= 0) continue;
        long long full = pax.passengerDelayMinutes(pr.first, pr.second);
        cands.push_back({pr.first, full});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        if (a.gain != b.gain) return a.gain > b.gain;
        return a.train < b.train;
    });

    int take = std::min(budget_k, static_cast<int>(cands.size()));
    for (int i = 0; i < take; ++i) plan.chosen_trains.push_back(cands[static_cast<size_t>(i)].train);

    for (int tid : plan.chosen_trains) {
        for (auto& pr : cur) {
            if (pr.first == tid) pr.second = 0;
        }
    }
    plan.passenger_delay_after = pdmOf(cur, pax);
    return plan;
}

int minInterventionsForThreshold(const std::vector<std::pair<int, int>>& affected_train_delay,
                                 const PassengerModel& pax, int max_k, long long threshold_pdm) {
    if (max_k < 0) return 0;
    long long lo = 0, hi = static_cast<long long>(max_k);
    long long ans = hi;
    while (lo <= hi) {
        long long mid = (lo + hi) / 2;
        InterventionPlan p = optimizeInterventionsGreedy(affected_train_delay, pax, static_cast<int>(mid));
        if (p.passenger_delay_after <= threshold_pdm) {
            ans = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    if (optimizeInterventionsGreedy(affected_train_delay, pax, static_cast<int>(ans)).passenger_delay_after >
        threshold_pdm)
        return -1;
    return static_cast<int>(ans);
}

}  // namespace rg
