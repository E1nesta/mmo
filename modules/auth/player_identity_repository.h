#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mmo::modules::auth {

struct PlayerIdentity {
    std::int64_t account_id{};
    std::int64_t player_id{};
};

class PlayerIdentityRepository {
public:
    virtual ~PlayerIdentityRepository() = default;

    virtual std::optional<PlayerIdentity> find_primary_by_account_id(
        std::int64_t account_id,
        std::string* error_message) = 0;
};

}  // namespace mmo::modules::auth
