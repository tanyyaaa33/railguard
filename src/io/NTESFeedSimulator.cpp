/*
 * Offline chronological replay of delay rows — O(N log N) sort on load, O(batch) per simulated day.
 */
#include "io/NTESFeedSimulator.h"

#include "io/CSVParser.h"

#include <algorithm>
#include <fstream>

namespace rg {

bool NTESFeedSimulator::load(const std::string& dataDir, std::string& err) {
    err.clear();
    rows_.clear();
    pos_ = 0;
    cur_date_.clear();
    std::string p = dataDir;
    if (!p.empty() && p.back() != '/') p += '/';
    std::ifstream f(p + "delays.csv");
    if (!f) {
        err = "cannot open delays.csv";
        return false;
    }
    std::string hdr;
    std::getline(f, hdr);
    std::string line;
    while (std::getline(f, line)) {
        auto row = parseCsvLine(line);
        if (row.size() < 4) continue;
        NtesRow r;
        r.train_id = std::stoi(row[0]);
        r.station_id = std::stoi(row[1]);
        r.date = row[2];
        r.delay_minutes = std::stoi(row[3]);
        rows_.push_back(std::move(r));
    }
    std::sort(rows_.begin(), rows_.end(), [](const NtesRow& a, const NtesRow& b) {
        if (a.date != b.date) return a.date < b.date;
        if (a.train_id != b.train_id) return a.train_id < b.train_id;
        return a.station_id < b.station_id;
    });
    return !rows_.empty();
}

void NTESFeedSimulator::reset() {
    pos_ = 0;
    cur_date_.clear();
}

bool NTESFeedSimulator::nextDay(std::vector<NtesRow>& batch, std::string& day_iso) {
    batch.clear();
    if (pos_ >= rows_.size()) return false;
    day_iso = rows_[pos_].date;
    cur_date_ = day_iso;
    while (pos_ < rows_.size() && rows_[pos_].date == day_iso) {
        batch.push_back(rows_[pos_]);
        ++pos_;
    }
    return true;
}

}  // namespace rg
