#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace runtime::entity {

enum class EntityKind : std::uint16_t {
    kPlayer = 1,
    kScene = 2,
    kInstance = 3,
    kChatRoom = 4,
};

struct EntityId {
    EntityKind kind{EntityKind::kPlayer};
    std::uint64_t key{};

    bool operator==(const EntityId& other) const {
        return kind == other.kind && key == other.key;
    }
};

struct EntityIdHash {
    std::size_t operator()(const EntityId& id) const {
        const auto kind = static_cast<std::uint64_t>(id.kind);
        return std::hash<std::uint64_t>{}((kind << 56U) ^ id.key);
    }
};

inline EntityId player_entity(std::int64_t player_id) {
    return EntityId{EntityKind::kPlayer, static_cast<std::uint64_t>(player_id)};
}

inline EntityId scene_entity(std::uint64_t scene_id) {
    return EntityId{EntityKind::kScene, scene_id};
}

inline EntityId instance_entity(std::uint64_t instance_id) {
    return EntityId{EntityKind::kInstance, instance_id};
}

inline EntityId chat_room_entity(std::uint64_t room_id) {
    return EntityId{EntityKind::kChatRoom, room_id};
}

std::string entity_kind_name(EntityKind kind);

}  // namespace runtime::entity
