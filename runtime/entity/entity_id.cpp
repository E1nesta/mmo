#include "runtime/entity/entity_id.h"

namespace runtime::entity {

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
    }
    return "unknown";
}

}  // namespace runtime::entity
