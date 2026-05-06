#include "runtime/scene/scene_fanout_budget.h"

#include <algorithm>

namespace runtime::scene {

SceneFanoutBudget::SceneFanoutBudget(SceneFanoutBudgetOptions options)
    : options_(options) {
    options_.max_snapshots_per_tick =
        std::max<std::size_t>(1, options_.max_snapshots_per_tick);
    options_.max_bytes_per_tick =
        std::max<std::size_t>(1, options_.max_bytes_per_tick);
}

bool SceneFanoutBudget::try_consume(std::size_t snapshots, std::size_t bytes) {
    if (snapshots_used_ + snapshots > options_.max_snapshots_per_tick ||
        bytes_used_ + bytes > options_.max_bytes_per_tick) {
        ++rejected_count_;
        return false;
    }
    snapshots_used_ += snapshots;
    bytes_used_ += bytes;
    return true;
}

void SceneFanoutBudget::reset() {
    snapshots_used_ = 0;
    bytes_used_ = 0;
}

std::size_t SceneFanoutBudget::snapshots_used() const {
    return snapshots_used_;
}

std::size_t SceneFanoutBudget::bytes_used() const {
    return bytes_used_;
}

std::uint64_t SceneFanoutBudget::rejected_count() const {
    return rejected_count_;
}

}  // namespace runtime::scene
