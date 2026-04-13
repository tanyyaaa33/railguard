#pragma once

#include <unordered_map>
#include <vector>

namespace rg {

struct TrainMeta {
    int train_id{-1};
    std::string name;
    int route_len{0};
};

/*
 * Synthetic passenger load per train from route length (no PAX column in CSV).
 * Passenger-delay-minutes = delay_minutes * passengers_on_train.
 *
 * References:
 * - Standard link-load modeling; cf. Cascetta, "Transportation Systems Engineering" (network assignment).
 */
class PassengerModel {
public:
    bool loadTrainsCsv(const std::string& dataDir, std::string& err);

    // passengers(train) = base + alpha * route_stops + beta * train_id (deterministic, reproducible).
    int passengersOnTrain(int train_id) const;

    long long passengerDelayMinutes(int train_id, int delay_minutes) const;

    const std::vector<TrainMeta>& trains() const { return trains_; }

private:
    std::vector<TrainMeta> trains_;
    std::unordered_map<int, size_t> id_index_;
};

long long totalPassengerDelay(const std::vector<std::pair<int, int>>& train_delay_minutes,
                              const PassengerModel& model);

}  // namespace rg
