#pragma once

#include <cstddef>
#include <cstdint>

namespace runtime::scene {

struct SceneFanoutBudgetOptions {
    std::size_t max_snapshots_per_tick{4096};
    std::size_t max_bytes_per_tick{1024 * 1024};
};

class SceneFanoutBudget {
public:
    explicit SceneFanoutBudget(SceneFanoutBudgetOptions options = {});

    bool try_consume(std::size_t snapshots, std::size_t bytes);
    void reset();

    std::size_t snapshots_used() const;
    std::size_t bytes_used() const;
    std::uint64_t rejected_count() const;

private:
    SceneFanoutBudgetOptions options_;
    std::size_t snapshots_used_{};
    std::size_t bytes_used_{};
    std::uint64_t rejected_count_{};
};

}  // namespace runtime::scene
