#pragma once

#include <string>
#include <vector>

namespace rg {

struct StationHotspot {
    int station_id{-1};
    std::string name;
    double fixed_window_mean{0};   // mean daily avg delay over last `fixed_days` calendar days
    double variable_window_mean{0};  // max mean over any contiguous subsequence (full history)
    int variable_best_len{0};
    int days_in_fixed{0};
};

/*
 * Hotspot detection on per-station daily average delay time series.
 * - Fixed window: last W calendar days of the dataset (ISO dates).
 * - Variable window: maximum average delay over any contiguous run of days (one station series).
 *
 * References:
 * - Sliding-window / two-pointer averaging — standard technique; see e.g. CLRS problem 9.2 (max subarray
 *   is different; here we maximize mean via enumerating lengths or prefix sums in O(n) per station).
 */
class SlidingWindowAnalyzer {
public:
    // Loads delays.csv; optional stations.csv for names (dataDir/trailing slash optional).
    bool load(const std::string& dataDir, std::string& err);

    // Stations with fixed-window mean strictly above threshold, sorted by mean desc; then top `limit`.
    std::vector<StationHotspot> hotspotsFixed(int fixed_days, double threshold_t, int limit) const;

    // Augment `out` (same station order as hotspotsFixed) with variable-window stats, or recompute all.
    void fillVariableWindowStats(std::vector<StationHotspot>& rows) const;

private:
    struct DayPoint {
        std::string date;  // ISO YYYY-MM-DD
        double avg_delay{0};
    };
    std::vector<std::vector<DayPoint>> by_station_;  // index = station_id
    std::vector<std::string> station_names_;
};

}  // namespace rg
