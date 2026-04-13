#pragma once

#include <string>
#include <vector>

namespace rg {

struct NtesRow {
    int train_id{-1};
    int station_id{-1};
    std::string date;
    int delay_minutes{0};
};

/*
 * Simulates a live NTES-style feed by replaying historical rows from delays.csv in chronological order.
 * (National Train Enquiry System — public running status; this is offline replay only.)
 */
class NTESFeedSimulator {
public:
    bool load(const std::string& dataDir, std::string& err);

    // Advance to next distinct date; returns false when exhausted.
    bool nextDay(std::vector<NtesRow>& batch, std::string& day_iso);

    void reset();

    const std::string& currentDate() const { return cur_date_; }

private:
    std::vector<NtesRow> rows_;
    size_t pos_{0};
    std::string cur_date_;
};

}  // namespace rg
