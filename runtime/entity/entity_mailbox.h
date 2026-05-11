#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include "runtime/entity/entity_id.h"

namespace runtime::entity {

struct EntityMessage {
    EntityId entity_id;
    std::uint32_t message_id{};
    std::uint64_t request_id{};
    std::uint64_t route_key{};
};

using EntityHandler = std::function<void(const EntityMessage&)>;

struct EntityTask {
    EntityMessage message;
    EntityHandler handler;
};

struct EntityMailboxOptions {
    std::size_t max_depth{1024};
};

class EntityMailbox {
public:
    explicit EntityMailbox(EntityMailboxOptions options = {});

    bool try_push(EntityTask task);
    std::optional<EntityTask> pop_task();
    bool try_remove(
        EntityId entity_id,
        std::uint32_t message_id,
        std::uint64_t route_key,
        std::uint64_t request_id);
    bool try_schedule_drain();
    void clear_scheduled_drain();
    bool try_acquire_writer();
    void release_writer();
    std::size_t size() const;
    bool has_pending_tasks() const;
    std::size_t max_depth() const;
    std::uint64_t dropped_count() const;

private:
    EntityMailboxOptions options_;
    mutable std::mutex mutex_;
    std::deque<EntityTask> queue_;
    std::uint64_t dropped_count_{};
    bool drain_scheduled_{};
    bool writer_active_{};
};

}  // namespace runtime::entity
