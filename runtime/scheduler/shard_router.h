#pragma once

#include <cstddef>
#include <cstdint>

namespace runtime::scheduler {

class ShardRouter {
public:
    explicit ShardRouter(std::size_t shard_count);

    std::size_t shard_for(std::uint64_t route_key) const;
    static std::size_t route(std::uint64_t route_key, std::size_t shard_count);

private:
    std::size_t shard_count_{};
};

}  // namespace runtime::scheduler
