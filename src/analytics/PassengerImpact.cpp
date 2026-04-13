/*
 * Synthetic load model for passenger-delay-minutes (deterministic from train metadata).
 */
#include "analytics/PassengerImpact.h"

#include "io/CSVParser.h"

#include <fstream>

namespace rg {

namespace {
constexpr int kBase = 650;
constexpr int kPerStop = 45;
constexpr int kPerTid = 12;
}  // namespace

bool PassengerModel::loadTrainsCsv(const std::string& dataDir, std::string& err) {
    trains_.clear();
    id_index_.clear();
    err.clear();
    std::string p = dataDir;
    if (!p.empty() && p.back() != '/') p += '/';
    std::ifstream f(p + "trains.csv");
    if (!f) {
        err = "cannot open trains.csv";
        return false;
    }
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        auto row = parseCsvLine(line);
        if (row.size() < 3) continue;
        TrainMeta tm;
        tm.train_id = std::stoi(row[0]);
        tm.name = row[1];
        int stops = 1;
        for (size_t i = 0; i < row[2].size(); ++i)
            if (row[2][i] == ';') ++stops;
        tm.route_len = stops;
        id_index_[tm.train_id] = trains_.size();
        trains_.push_back(std::move(tm));
    }
    return !trains_.empty();
}

int PassengerModel::passengersOnTrain(int train_id) const {
    auto it = id_index_.find(train_id);
    if (it == id_index_.end()) return kBase;
    const TrainMeta& tm = trains_[it->second];
    return kBase + kPerStop * tm.route_len + kPerTid * tm.train_id;
}

long long PassengerModel::passengerDelayMinutes(int train_id, int delay_minutes) const {
    return static_cast<long long>(passengersOnTrain(train_id)) * static_cast<long long>(delay_minutes);
}

long long totalPassengerDelay(const std::vector<std::pair<int, int>>& train_delay_minutes,
                              const PassengerModel& model) {
    long long s = 0;
    for (const auto& pr : train_delay_minutes) s += model.passengerDelayMinutes(pr.first, pr.second);
    return s;
}

}  // namespace rg
