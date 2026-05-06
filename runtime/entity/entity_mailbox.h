#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

#include "runtime/entity/entity_id.h"
#include "runtime/protocol/frame.h"

namespace runtime::entity {

struct EntityMessage {
    EntityId entity_id;
    std::uint32_t message_id{};
    std::uint64_t route_key{};
    runtime::protocol::FrameMessage frame;
};

struct EntityMailboxOptions {
    std::size_t max_depth{1024};
};

class EntityMailbox {
public:
    explicit EntityMailbox(EntityMailboxOptions options = {});

    bool try_push(EntityMessage message);
    std::optional<EntityMessage> pop();
    std::size_t size() const;
    std::size_t max_depth() const;
    std::uint64_t dropped_count() const;

private:
    EntityMailboxOptions options_;
    mutable std::mutex mutex_;
    std::deque<EntityMessage> queue_;
    std::uint64_t dropped_count_{};
};

}  // namespace runtime::entity
