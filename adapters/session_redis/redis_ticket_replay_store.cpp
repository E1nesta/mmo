#include "adapters/session_redis/redis_ticket_replay_store.h"

#include <utility>

#include "runtime/storage/redis_keys.h"

namespace adapters::session_redis {

RedisTicketReplayStore::RedisTicketReplayStore(
    std::shared_ptr<runtime::storage::RedisConnectionPool> pool)
    : pool_(std::move(pool)) {}

bool RedisTicketReplayStore::consume(
    const std::string& ticket_id,
    std::uint64_t now_millis,
    std::uint64_t expire_at_millis) {
    if (pool_ == nullptr || ticket_id.empty() || expire_at_millis <= now_millis) {
        return false;
    }
    const auto client = pool_->acquire();
    bool stored = false;
    std::string error_message;
    if (!client->set_if_absent_with_ttl_millis(
            runtime::storage::ticket_replay_key(ticket_id),
            "1",
            expire_at_millis - now_millis,
            &stored,
            &error_message)) {
        return false;
    }
    return stored;
}

}  // namespace adapters::session_redis
