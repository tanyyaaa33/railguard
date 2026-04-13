#pragma once

#include <vector>

namespace rg {

class PassengerModel;

struct InterventionPlan {
    std::vector<int> chosen_trains;  // train_ids to intervene (greedy order)
    long long passenger_delay_before{0};
    long long passenger_delay_after{0};
};

/*
 * Reduce passenger-delay-minutes (PDM) under a budget of K interventions.
 * Each intervention clears delay on one train to 0 (hold resolved / reroute success).
 *
 * Strategy:
 * - Greedy by descending marginal PDM reduction (delay_t * passengers_t) — O(T log T) for T affected trains.
 * - To hit a target threshold T_pdm, binary-search the smallest k in [0, K] using the greedy prefix of size k;
 *   PDM after interventions is monotone non-increasing in k, so binary search is valid for the greedy policy.
 *
 * References:
 * - Greedy approximation for submodular-like coverage (not proved submodular here; heuristic justified for ops).
 * - T. H. Cormen et al., CLRS 3rd ed., §11 (hashing) N/A; binary search: §2.3-2.4.
 */
InterventionPlan optimizeInterventionsGreedy(const std::vector<std::pair<int, int>>& affected_train_delay,
                                             const PassengerModel& pax, int budget_k);

// Smallest k in [0, max_k] s.t. greedy plan with k interventions yields PDM <= threshold (or max_k if never).
int minInterventionsForThreshold(const std::vector<std::pair<int, int>>& affected_train_delay,
                                 const PassengerModel& pax, int max_k, long long threshold_pdm);

}  // namespace rg
