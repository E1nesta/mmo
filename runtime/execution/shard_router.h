#pragma once

#include <cstddef>
#include <cstdint>

namespace runtime::execution {

struct ShardTopology {
    std::size_t player_shards{};
    std::size_t scene_shards{};
    std::size_t instance_shards{};
};

class ShardRouter {
public:
    explicit ShardRouter(ShardTopology topology);

    std::size_t player_shard(std::int64_t player_id) const;
    std::size_t scene_shard(std::int64_t scene_id) const;
    std::size_t instance_shard(std::int64_t instance_id) const;

private:
    static std::size_t route(std::int64_t key, std::size_t shard_count);

    ShardTopology topology_;
};

}  // namespace runtime::execution
