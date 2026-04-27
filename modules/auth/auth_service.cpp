#include "modules/auth/auth_service.h"

#include <functional>

namespace mmo::modules::auth {

LoginResult AuthService::login(
    const std::string& account_name,
    const std::string& device_id) const {
    const auto seed = static_cast<std::int64_t>(
        std::hash<std::string>{}(account_name + ":" + device_id) % 1000000);
    LoginResult result;
    result.account_id = 100000 + seed;
    result.player_id = 200000 + seed;
    result.session_token = "session-" + std::to_string(result.player_id);
    result.expires_at_epoch_seconds = 4102444800LL;
    return result;
}

}  // namespace mmo::modules::auth
