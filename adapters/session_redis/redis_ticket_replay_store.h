#pragma once

#include <memory>

#include "runtime/session/ticket_replay_guard.h"
#include "runtime/storage/redis_connection_pool.h"

namespace mmo::adapters::session_redis {

class RedisTicketReplayStore final
    : public mmo::runtime::session::TicketReplayStore {
public:
    explicit RedisTicketReplayStore(
        std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool);

    bool consume(
        const std::string& ticket_id,
        std::uint64_t now_millis,
        std::uint64_t expire_at_millis) override;

private:
    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool_;
};

}  // namespace mmo::adapters::session_redis
