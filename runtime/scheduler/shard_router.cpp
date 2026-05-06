#include "runtime/scheduler/shard_router.h"

#include <stdexcept>

namespace runtime::scheduler {

ShardRouter::ShardRouter(std::size_t shard_count) : shard_count_(shard_count) {
    if (shard_count_ == 0) {
        throw std::invalid_argument("shard count must be greater than zero");
    }
}

std::size_t ShardRouter::shard_for(std::uint64_t route_key) const {
    return route(route_key, shard_count_);
}

std::size_t ShardRouter::route(std::uint64_t route_key, std::size_t shard_count) {
    if (shard_count == 0) {
        throw std::invalid_argument("shard count must be greater than zero");
    }
    return static_cast<std::size_t>(route_key % shard_count);
}

}  // namespace runtime::scheduler
