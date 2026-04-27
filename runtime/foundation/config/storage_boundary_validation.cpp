// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/foundation/config/storage_boundary_validation.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace common::config {

namespace {

bool HasKeyWithPrefix(const SimpleConfig& config, const std::string& prefix) {
    for (const auto& entry : config.Values()) {
        if (entry.first.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool RequireAnyPrefix(const SimpleConfig& config,
                      std::initializer_list<std::string> prefixes,
                      const std::string& description,
                      std::string* error_message) {
    for (const auto& prefix : prefixes) {
        if (HasKeyWithPrefix(config, prefix)) {
            return true;
        }
    }
    if (error_message != nullptr) {
        *error_message = description;
    }
    return false;
}

bool ForbidPrefixes(const SimpleConfig& config,
                    std::initializer_list<std::string> prefixes,
                    const std::string& service_name,
                    std::string* error_message) {
    for (const auto& prefix : prefixes) {
        if (!HasKeyWithPrefix(config, prefix)) {
            continue;
        }
        if (error_message != nullptr) {
            *error_message = service_name + " must not define " + prefix + "*";
        }
        return false;
    }
    return true;
}

}  // namespace

bool ValidateAuthStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.account.mysql."},
               "auth_server requires storage.account.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.player.mysql."},
               "auth_server requires storage.player.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.session.redis.", "storage.account.redis."},
               "auth_server requires storage.session.redis.* or storage.account.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.account.mysql.reader.",
                           "storage.player.mysql.reader.",
                           "storage.battle.mysql.",
                           "storage.social.mysql.",
                           "storage.player_cache.redis.",
                           "storage.runtime.redis.",
                           "storage.battle.redis.",
                           "storage.player.redis.",
                           "storage.redis."},
                          "auth_server",
                          error_message);
}

bool ValidateApiGatewayStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.session.redis.", "storage.redis."},
               "api_gateway_server requires storage.session.redis.* or storage.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.account.mysql.",
                           "storage.player.mysql.",
                           "storage.battle.mysql.",
                           "storage.social.mysql.",
                           "storage.account.redis.",
                           "storage.player.redis.",
                           "storage.battle.redis.",
                           "storage.player_cache.redis.",
                           "storage.runtime.redis."},
                          "api_gateway_server",
                          error_message);
}

bool ValidatePlayerQueryStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.player.mysql."},
               "player_query_server requires storage.player.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.player_cache.redis.", "storage.player.redis."},
               "player_query_server requires storage.player_cache.redis.* or storage.player.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.account.mysql.",
                           "storage.battle.mysql.",
                           "storage.social.mysql.",
                           "storage.session.redis.",
                           "storage.account.redis.",
                           "storage.runtime.redis.",
                           "storage.battle.redis.",
                           "storage.redis."},
                          "player_query_server",
                          error_message);
}

bool ValidatePlayerWriteStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.player.mysql."},
               "player_write_grpc_server requires storage.player.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.player_cache.redis.", "storage.player.redis."},
               "player_write_grpc_server requires storage.player_cache.redis.* or storage.player.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.player.mysql.reader.",
                           "storage.account.mysql.",
                           "storage.battle.mysql.",
                           "storage.social.mysql.",
                           "storage.session.redis.",
                           "storage.account.redis.",
                           "storage.runtime.redis.",
                           "storage.battle.redis.",
                           "storage.redis."},
                          "player_write_grpc_server",
                          error_message);
}

bool ValidateDungeonRuntimeStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.battle.mysql."},
               "dungeon_runtime_server requires storage.battle.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.runtime.redis.", "storage.battle.redis."},
               "dungeon_runtime_server requires storage.runtime.redis.* or storage.battle.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.battle.mysql.reader.",
                           "storage.account.mysql.",
                           "storage.player.mysql.",
                           "storage.social.mysql.",
                           "storage.session.redis.",
                           "storage.account.redis.",
                           "storage.player_cache.redis.",
                           "storage.player.redis.",
                           "storage.redis."},
                          "dungeon_runtime_server",
                          error_message);
}

bool ValidateOnlineGatewayStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.session.redis.", "storage.redis."},
               "online_gateway_server requires storage.session.redis.* or storage.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.account.mysql.",
                           "storage.player.mysql.",
                           "storage.battle.mysql.",
                           "storage.social.mysql.",
                           "storage.account.redis.",
                           "storage.player.redis.",
                           "storage.battle.redis.",
                           "storage.player_cache.redis.",
                           "storage.runtime.redis."},
                          "online_gateway_server",
                          error_message);
}

bool ValidateSocialStorageConfig(const SimpleConfig& config, std::string* error_message) {
    return RequireAnyPrefix(
               config,
               {"storage.social.mysql."},
               "social_server requires storage.social.mysql.*",
               error_message) &&
           RequireAnyPrefix(
               config,
               {"storage.session.redis.", "storage.redis."},
               "social_server requires storage.session.redis.* or storage.redis.*",
               error_message) &&
           ForbidPrefixes(config,
                          {"storage.social.mysql.reader.",
                           "storage.account.mysql.",
                           "storage.player.mysql.",
                           "storage.battle.mysql.",
                           "storage.account.redis.",
                           "storage.player.redis.",
                           "storage.player_cache.redis.",
                           "storage.runtime.redis.",
                           "storage.battle.redis."},
                          "social_server",
                          error_message);
}

}  // namespace common::config
