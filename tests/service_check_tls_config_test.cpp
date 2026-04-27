#include "tests/tls_test_materials.h"

#include <filesystem>
#include <iostream>
#include <string>

#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

class TempDirectory {
public:
    TempDirectory() {
        char pattern[] = "/tmp/mobile-game-backend-service-check-XXXXXX";
        const auto* created = mkdtemp(pattern);
        if (created != nullptr) {
            path_ = created;
        }
    }

    ~TempDirectory() {
        if (!path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }

    [[nodiscard]] bool valid() const { return !path_.empty(); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::string ShellQuote(const std::string& text) {
    std::string quoted = "'";
    for (const char character : text) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

int RunCommand(const std::string& command) {
    const int status = std::system(command.c_str());
    if (status < 0) {
        return status;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
}

std::string BuildDeliveryEnv(const std::filesystem::path& fixture_dir, bool include_shared_secret) {
    std::string environment;
    if (include_shared_secret) {
        environment += "TRUSTED_GATEWAY_SHARED_SECRET=test-shared-secret ";
    }
    environment += "TRANSPORT_TLS_CERT_FILE=" + ShellQuote((fixture_dir / "server.crt").string()) + ' ';
    environment += "TRANSPORT_TLS_KEY_FILE=" + ShellQuote((fixture_dir / "server.key").string()) + ' ';
    environment += "TRANSPORT_TLS_CA_FILE=" + ShellQuote((fixture_dir / "ca.pem").string()) + ' ';
    environment += "UPSTREAM_TLS_CA_FILE=" + ShellQuote((fixture_dir / "ca.pem").string()) + ' ';
    environment += "CLIENT_TLS_CA_FILE=" + ShellQuote((fixture_dir / "ca.pem").string()) + ' ';
    return environment;
}

}  // namespace

int main(int /*argc*/, char* argv[]) {
    const auto build_dir = std::filesystem::canonical(std::filesystem::path(argv[0])).parent_path();
    const auto service_check = build_dir / "service_check";
    const auto project_root = std::filesystem::path(PROJECT_SOURCE_DIR);
    const auto delivery_online = project_root / "configs/delivery/online_gateway_server.conf";
    const auto delivery_auth = project_root / "configs/delivery/auth_server.conf";
    const auto delivery_player_query = project_root / "configs/delivery/player_query_server.conf";
    const auto delivery_dungeon = project_root / "configs/delivery/dungeon_runtime_server.conf";
    const auto prod_online = project_root / "configs/prod/online_gateway_server.conf";
    const auto prod_auth = project_root / "configs/prod/auth_server.conf";
    const auto prod_player_query = project_root / "configs/prod/player_query_server.conf";
    const auto prod_dungeon = project_root / "configs/prod/dungeon_runtime_server.conf";

    TempDirectory temp_directory;
    if (!Expect(temp_directory.valid(), "expected temporary directory to be created")) {
        return 1;
    }
    if (!Expect(test::tls::WriteFixtureSet(temp_directory.path()), "expected TLS fixture files to be written")) {
        return 1;
    }

    const auto auth_missing_secret = RunCommand(
        ShellQuote(service_check.string()) + " --config " + ShellQuote(delivery_auth.string()) + " --mode config");
    if (!Expect(auth_missing_secret != 0, "expected delivery auth config to fail without shared secret")) {
        return 1;
    }

    const auto online_missing_ca = RunCommand(
        "TRANSPORT_TLS_CERT_FILE=" + ShellQuote((temp_directory.path() / "server.crt").string()) + ' ' +
        "TRANSPORT_TLS_KEY_FILE=" + ShellQuote((temp_directory.path() / "server.key").string()) + ' ' +
        ShellQuote(service_check.string()) + " --config " + ShellQuote(delivery_online.string()) + " --mode config");
    if (!Expect(online_missing_ca != 0, "expected delivery online gateway config to fail without transport CA")) {
        return 1;
    }

    const auto auth_missing_cert = RunCommand(
        "TRUSTED_GATEWAY_SHARED_SECRET=test-shared-secret " +
        ShellQuote(service_check.string()) + " --config " + ShellQuote(delivery_auth.string()) + " --mode config");
    if (!Expect(auth_missing_cert != 0, "expected delivery auth config to fail without transport cert/key")) {
        return 1;
    }

    const auto delivery_environment = BuildDeliveryEnv(temp_directory.path(), true);
    if (!Expect(
            RunCommand(delivery_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(delivery_online.string()) + " --mode config") == 0,
            "expected delivery online gateway config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(delivery_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(delivery_auth.string()) + " --mode config") == 0,
            "expected delivery auth config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(delivery_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(delivery_player_query.string()) + " --mode config") == 0,
            "expected delivery player query config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(delivery_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(delivery_dungeon.string()) + " --mode config") == 0,
            "expected delivery dungeon runtime config to pass with complete TLS files")) {
        return 1;
    }

    const auto production_environment =
        delivery_environment +
        "MYSQL_PASSWORD=prod-db-secret "
        "REDIS_PASSWORD=prod-redis-secret "
        "ACCOUNT_MYSQL_PASSWORD=prod-account-db-secret "
        "PLAYER_MYSQL_PASSWORD=prod-player-db-secret "
        "PLAYER_MYSQL_READER_PASSWORD=prod-player-reader-db-secret "
        "BATTLE_MYSQL_PASSWORD=prod-battle-db-secret "
        "ACCOUNT_REDIS_PASSWORD=prod-account-redis-secret "
        "PLAYER_REDIS_PASSWORD=prod-player-redis-secret "
        "BATTLE_REDIS_PASSWORD=prod-battle-redis-secret ";
    if (!Expect(
            RunCommand(production_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(prod_online.string()) + " --mode config") == 0,
            "expected production online gateway config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(production_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(prod_auth.string()) + " --mode config") == 0,
            "expected production auth config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(production_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(prod_player_query.string()) + " --mode config") == 0,
            "expected production player query config to pass with complete TLS files")) {
        return 1;
    }
    if (!Expect(
            RunCommand(production_environment + ShellQuote(service_check.string()) + " --config " +
                       ShellQuote(prod_dungeon.string()) + " --mode config") == 0,
            "expected production dungeon runtime config to pass with complete TLS files")) {
        return 1;
    }

    return 0;
}
