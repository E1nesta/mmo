#include "runtime/execution/shard_router.h"

#include <stdexcept>

namespace runtime::execution {

ShardRouter::ShardRouter(ShardTopology topology) : topology_(topology) {
    if (topology_.player_shards == 0 || topology_.scene_shards == 0 ||
        topology_.instance_shards == 0) {
        throw std::invalid_argument("shard counts must be greater than zero");
    }
}

std::size_t ShardRouter::player_shard(std::int64_t player_id) const {
    return route(player_id, topology_.player_shards);
}

std::size_t ShardRouter::scene_shard(std::int64_t scene_id) const {
    return route(scene_id, topology_.scene_shards);
}

std::size_t ShardRouter::instance_shard(std::int64_t instance_id) const {
    return route(instance_id, topology_.instance_shards);
}

std::size_t ShardRouter::route(std::int64_t key, std::size_t shard_count) {
    const auto raw = static_cast<std::uint64_t>(key);
    const auto value = key >= 0 ? raw : 0ULL - raw;
    return static_cast<std::size_t>(value % shard_count);
}

}  // namespace runtime::execution
