/*
 * Load + aggregate delays to daily per-station means; fixed tail window O(S);
 * variable-window max mean over contiguous days O(D^2) per station (D <= days in file).
 *
 * References: prefix sums / interval means — standard; cf. Kadane-related exposition in CLRS (max subarray).
 */
#include "analytics/SlidingWindow.h"

#include "io/CSVParser.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>

namespace rg {

namespace {

std::string normDir(std::string d) {
    if (!d.empty() && d.back() != '/') d += '/';
    return d;
}

}  // namespace

bool SlidingWindowAnalyzer::load(const std::string& dataDir, std::string& err) {
    err.clear();
    by_station_.clear();
    station_names_.clear();
    const std::string p = normDir(dataDir);

    {
        std::ifstream fs(p + "stations.csv");
        if (fs) {
            std::string line;
            std::getline(fs, line);
            while (std::getline(fs, line)) {
                auto row = parseCsvLine(line);
                if (row.size() < 2) continue;
                int sid = std::stoi(row[0]);
                if (static_cast<int>(station_names_.size()) <= sid) station_names_.resize(static_cast<size_t>(sid + 1));
                station_names_[static_cast<size_t>(sid)] = row[1];
            }
        }
    }

    // station,date -> sum, count
    std::unordered_map<std::string, std::pair<long long, int>> bucket;
    std::ifstream fd(p + "delays.csv");
    if (!fd) {
        err = "cannot open delays.csv";
        return false;
    }
    std::string hdr;
    std::getline(fd, hdr);
    std::string line;
    while (std::getline(fd, line)) {
        auto row = parseCsvLine(line);
        if (row.size() < 4) continue;
        int sid = std::stoi(row[1]);
        const std::string& ds = row[2];
        int dm = std::stoi(row[3]);
        std::string key = std::to_string(sid) + "|" + ds;
        auto& pr = bucket[key];
        pr.first += dm;
        pr.second += 1;
    }

    int max_sid = -1;
    for (const auto& kv : bucket) {
        size_t sep = kv.first.find('|');
        int sid = std::stoi(kv.first.substr(0, sep));
        max_sid = std::max(max_sid, sid);
    }
    if (max_sid < 0) {
        err = "no delay rows";
        return false;
    }
    by_station_.assign(static_cast<size_t>(max_sid + 1), {});

    for (const auto& kv : bucket) {
        size_t sep = kv.first.find('|');
        int sid = std::stoi(kv.first.substr(0, sep));
        std::string ds = kv.first.substr(sep + 1);
        double avg = static_cast<double>(kv.second.first) / static_cast<double>(kv.second.second);
        by_station_[static_cast<size_t>(sid)].push_back({std::move(ds), avg});
    }

    for (auto& series : by_station_) {
        std::sort(series.begin(), series.end(),
                  [](const DayPoint& a, const DayPoint& b) { return a.date < b.date; });
    }

    return true;
}

void SlidingWindowAnalyzer::fillVariableWindowStats(std::vector<StationHotspot>& rows) const {
    for (auto& h : rows) {
        const auto& series = by_station_[static_cast<size_t>(h.station_id)];
        if (series.empty()) {
            h.variable_window_mean = 0;
            h.variable_best_len = 0;
            continue;
        }
        // Prefix sums for O(1) interval means — O(n^2) over lengths; n <= 180 here.
        std::vector<double> pref(series.size() + 1, 0);
        for (size_t i = 0; i < series.size(); ++i) pref[i + 1] = pref[i] + series[i].avg_delay;
        double best = 0;
        int best_len = 0;
        for (size_t len = 1; len <= series.size(); ++len) {
            for (size_t i = 0; i + len <= series.size(); ++i) {
                double mean = (pref[i + len] - pref[i]) / static_cast<double>(len);
                if (mean > best || (std::abs(mean - best) < 1e-9 && static_cast<int>(len) > best_len)) {
                    best = mean;
                    best_len = static_cast<int>(len);
                }
            }
        }
        h.variable_window_mean = best;
        h.variable_best_len = best_len;
    }
}

std::vector<StationHotspot> SlidingWindowAnalyzer::hotspotsFixed(int fixed_days, double threshold_t,
                                                                 int limit) const {
    (void)threshold_t;  // CLI may print "hotspot" flag when mean > T; ranking uses mean regardless.
    std::vector<StationHotspot> out;
    if (fixed_days <= 0 || by_station_.empty()) return out;

    for (size_t sid = 0; sid < by_station_.size(); ++sid) {
        const auto& series = by_station_[sid];
        if (series.empty()) continue;
        int take = std::min(fixed_days, static_cast<int>(series.size()));
        double sum = 0;
        for (int i = 0; i < take; ++i)
            sum += series[series.size() - static_cast<size_t>(take) + static_cast<size_t>(i)].avg_delay;
        double mean = sum / static_cast<double>(take);
        StationHotspot h;
        h.station_id = static_cast<int>(sid);
        if (sid < station_names_.size()) h.name = station_names_[sid];
        h.fixed_window_mean = mean;
        h.days_in_fixed = take;
        out.push_back(std::move(h));
    }

    std::sort(out.begin(), out.end(), [](const StationHotspot& a, const StationHotspot& b) {
        if (a.fixed_window_mean != b.fixed_window_mean) return a.fixed_window_mean > b.fixed_window_mean;
        return a.station_id < b.station_id;
    });

    if (static_cast<int>(out.size()) > limit) out.resize(static_cast<size_t>(limit));
    return out;
}

}  // namespace rg
