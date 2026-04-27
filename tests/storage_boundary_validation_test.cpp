#include "runtime/foundation/config/simple_config.h"
#include "runtime/foundation/config/storage_boundary_validation.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

common::config::SimpleConfig LoadConfig(const std::string& filename, const std::string& contents) {
    std::ofstream output(filename);
    output << contents;
    output.close();

    common::config::SimpleConfig config;
    (void)config.LoadFromFile(filename);
    std::filesystem::remove(filename);
    return config;
}

}  // namespace

int main() {
    {
        const auto config = LoadConfig(
            "auth_storage_validation.conf",
            "storage.account.mysql.writer.host=127.0.0.1\n"
            "storage.player.mysql.writer.host=127.0.0.1\n"
            "storage.session.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(common::config::ValidateAuthStorageConfig(config, &error),
                    "expected auth storage config to pass: " + error)) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "auth_storage_validation_invalid.conf",
            "storage.account.mysql.writer.host=127.0.0.1\n"
            "storage.player.mysql.writer.host=127.0.0.1\n"
            "storage.session.redis.host=127.0.0.1\n"
            "storage.battle.mysql.writer.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidateAuthStorageConfig(config, &error) &&
                        error.find("storage.battle.mysql.") != std::string::npos,
                    "expected auth storage config to reject battle mysql prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "api_gateway_storage_validation.conf",
            "storage.session.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(common::config::ValidateApiGatewayStorageConfig(config, &error),
                    "expected api gateway storage config to pass: " + error)) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "api_gateway_storage_validation_invalid.conf",
            "storage.session.redis.host=127.0.0.1\n"
            "storage.player_cache.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidateApiGatewayStorageConfig(config, &error) &&
                        error.find("storage.player_cache.redis.") != std::string::npos,
                    "expected api gateway storage config to reject player cache redis prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "player_query_storage_validation.conf",
            "storage.player.mysql.writer.host=127.0.0.1\n"
            "storage.player.mysql.reader.host=127.0.0.1\n"
            "storage.player_cache.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(common::config::ValidatePlayerQueryStorageConfig(config, &error),
                    "expected player_query storage config to pass: " + error)) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "player_query_storage_validation_invalid.conf",
            "storage.player.mysql.writer.host=127.0.0.1\n"
            "storage.player_cache.redis.host=127.0.0.1\n"
            "storage.session.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidatePlayerQueryStorageConfig(config, &error) &&
                        error.find("storage.session.redis.") != std::string::npos,
                    "expected player_query storage config to reject session redis prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "player_write_storage_validation_invalid.conf",
            "storage.player.mysql.writer.host=127.0.0.1\n"
            "storage.player.mysql.reader.host=127.0.0.1\n"
            "storage.player_cache.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidatePlayerWriteStorageConfig(config, &error) &&
                        error.find("storage.player.mysql.reader.") != std::string::npos,
                    "expected player_write storage config to reject reader prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "dungeon_storage_validation_invalid.conf",
            "storage.battle.mysql.writer.host=127.0.0.1\n"
            "storage.battle.mysql.reader.host=127.0.0.1\n"
            "storage.runtime.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidateDungeonRuntimeStorageConfig(config, &error) &&
                        error.find("storage.battle.mysql.reader.") != std::string::npos,
                    "expected dungeon storage config to reject battle reader prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "online_storage_validation.conf",
            "storage.session.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(common::config::ValidateOnlineGatewayStorageConfig(config, &error),
                    "expected online gateway storage config to pass: " + error)) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "online_storage_validation_invalid.conf",
            "storage.session.redis.host=127.0.0.1\n"
            "storage.player.mysql.writer.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidateOnlineGatewayStorageConfig(config, &error) &&
                        error.find("storage.player.mysql.") != std::string::npos,
                    "expected online gateway storage config to reject mysql prefix")) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "social_storage_validation.conf",
            "storage.social.mysql.writer.host=127.0.0.1\n"
            "storage.session.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(common::config::ValidateSocialStorageConfig(config, &error),
                    "expected social storage config to pass: " + error)) {
            return 1;
        }
    }

    {
        const auto config = LoadConfig(
            "social_storage_validation_invalid.conf",
            "storage.social.mysql.writer.host=127.0.0.1\n"
            "storage.session.redis.host=127.0.0.1\n"
            "storage.player_cache.redis.host=127.0.0.1\n");
        std::string error;
        if (!Expect(!common::config::ValidateSocialStorageConfig(config, &error) &&
                        error.find("storage.player_cache.redis.") != std::string::npos,
                    "expected social storage config to reject player cache redis prefix")) {
            return 1;
        }
    }

    return 0;
}
