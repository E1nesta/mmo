#include "runtime/entity/entity_id.h"

namespace runtime::entity {

bool is_valid_entity_kind(EntityKind kind) {
    switch (kind) {
        case EntityKind::kPlayer:
        case EntityKind::kScene:
        case EntityKind::kInstance:
        case EntityKind::kChatRoom:
        case EntityKind::kWorld:
            return true;
    }
    return false;
}

bool is_valid_entity_id(EntityId id) {
    return is_valid_entity_kind(id.kind) && id.key != 0U;
}

std::string entity_kind_name(EntityKind kind) {
    switch (kind) {
        case EntityKind::kPlayer:
            return "player";
        case EntityKind::kScene:
            return "scene";
        case EntityKind::kInstance:
            return "instance";
        case EntityKind::kChatRoom:
            return "chat_room";
        case EntityKind::kWorld:
            return "world";
    }
    return "unknown";
}

}  // namespace runtime::entity
