#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "modules/auth/auth_service.h"
#include "modules/auth/mysql_account_repository.h"
#include "modules/auth/mysql_player_identity_repository.h"
#include "modules/auth/password_hasher.h"
#include "runtime/foundation/server_config.h"
#include "runtime/storage/storage_runtime.h"

namespace {

void expect_login_failure(const modules::auth::LoginResult& result) {
    assert(!result.success);
    assert(result.error_code == 401);
    assert(result.error_message == "login authentication failed");
    assert(result.account_id == 0);
    assert(result.player_id == 0);
    assert(result.session_token.empty());
}

}  // namespace

int main() {
    modules::auth::PasswordHash password_hash;
    std::string error;
    assert(modules::auth::PasswordHasher::hash_password(
        "demo_password",
        "00112233445566778899aabbccddeeff",
        10000,
        &password_hash,
        &error));
    assert(password_hash.hash_hex ==
           "d8209a2c86e5d177389cbd88d1e7d836a98301af8e320d197249a4bb8012861b");
    assert(modules::auth::PasswordHasher::verify_password(
        "demo_password", password_hash));
    assert(!modules::auth::PasswordHasher::verify_password(
        "wrong_password", password_hash));

    const auto config = runtime::foundation::load_server_config_from_env();
    std::shared_ptr<runtime::storage::MysqlConnectionPool> mysql_pool;
    assert(runtime::storage::initialize_mysql_pool(
        config, &mysql_pool, &error));

    auto accounts =
        std::make_shared<modules::auth::MysqlAccountRepository>(mysql_pool);
    auto identities =
        std::make_shared<modules::auth::MysqlPlayerIdentityRepository>(
            mysql_pool);
    modules::auth::AuthService service(accounts, identities);

    const auto ok =
        service.login("demo_player", "demo_password", "auth-data-probe");
    assert(ok.success);
    assert(ok.account_id == 1098216);
    assert(ok.player_id == 1198216);
    assert(!ok.session_token.empty());

    const auto wrong_password =
        service.login("demo_player", "wrong_password", "auth-data-probe");
    expect_login_failure(wrong_password);
    assert(wrong_password.internal_reason == "password is invalid");

    const auto banned =
        service.login("demo_banned", "demo_password", "auth-data-probe");
    expect_login_failure(banned);
    assert(banned.internal_reason == "account is banned");

    const auto missing_identity = service.login(
        "demo_missing_identity", "demo_password", "auth-data-probe");
    expect_login_failure(missing_identity);
    assert(missing_identity.internal_reason == "player identity is missing");

    std::cout << "auth data probe ok\n";
    return 0;
}
