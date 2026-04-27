#include "runtime/foundation/config/simple_config.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/observability/structured_log.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/storage/mysql/mysql_client.h"
#include "runtime/storage/redis/redis_client.h"
#include "runtime/transport/tls_options.h"
#include "runtime/transport/transport_client.h"
#include "tools/client_channel_support.h"
#include "tools/demo_support.h"

#include "game_backend.pb.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

enum class DemoScenario {
    kFull,
    kLoginOnly,
    kLoadOnly,
};

struct DemoOptions {
    std::string config_profile = "local";
    std::string gate_config = "configs/local/gateway_client.conf";
    std::string api_config = "configs/local/api_gateway_client.conf";
    std::string auth_config = "configs/auth_server.conf";
    std::string player_query_config = "configs/player_query_server.conf";
    std::string dungeon_config = "configs/dungeon_runtime_server.conf";
    std::string account_name = "demo";
    std::string password = "demo123";
    bool reset_demo_state = true;
    bool run_negative_cases = true;
    bool verify_session_recovery = true;
    DemoScenario scenario = DemoScenario::kFull;
    std::string session_id;
    std::int64_t player_id = 0;
    std::optional<common::error::ErrorCode> expected_load_error;
    std::optional<common::error::ErrorCode> expected_enter_error;
};

struct CallResult {
    bool ok = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    common::net::Packet packet;
};

void EmitToolLog(common::log::LogLevel level, const framework::observability::LogEntry& entry) {
    common::log::Logger::Instance().Log(level, entry.Build());
}

framework::observability::LogEntry NewToolEvent(std::string_view event) {
    framework::observability::LogEntry entry;
    entry.Add("event", event);
    return entry;
}

std::optional<common::error::ErrorCode> ParseErrorCode(std::string_view value) {
    using common::error::ErrorCode;

    if (value == "OK") {
        return ErrorCode::kOk;
    }
    if (value == "ACCOUNT_NOT_FOUND") {
        return ErrorCode::kAccountNotFound;
    }
    if (value == "ACCOUNT_DISABLED") {
        return ErrorCode::kAccountDisabled;
    }
    if (value == "INVALID_PASSWORD") {
        return ErrorCode::kInvalidPassword;
    }
    if (value == "SESSION_INVALID") {
        return ErrorCode::kSessionInvalid;
    }
    if (value == "PLAYER_NOT_FOUND") {
        return ErrorCode::kPlayerNotFound;
    }
    if (value == "PLAYER_BUSY") {
        return ErrorCode::kPlayerBusy;
    }
    if (value == "STAGE_NOT_FOUND") {
        return ErrorCode::kStageNotFound;
    }
    if (value == "STAGE_LOCKED") {
        return ErrorCode::kStageLocked;
    }
    if (value == "STAMINA_NOT_ENOUGH") {
        return ErrorCode::kStaminaNotEnough;
    }
    if (value == "BATTLE_NOT_FOUND") {
        return ErrorCode::kBattleNotFound;
    }
    if (value == "BATTLE_MISMATCH") {
        return ErrorCode::kBattleMismatch;
    }
    if (value == "BATTLE_ALREADY_SETTLED") {
        return ErrorCode::kBattleAlreadySettled;
    }
    if (value == "INVALID_STAR") {
        return ErrorCode::kInvalidStar;
    }
    if (value == "STORAGE_ERROR") {
        return ErrorCode::kStorageError;
    }
    if (value == "SERVICE_UNAVAILABLE") {
        return ErrorCode::kServiceUnavailable;
    }
    if (value == "UPSTREAM_TIMEOUT") {
        return ErrorCode::kUpstreamTimeout;
    }
    if (value == "BAD_GATEWAY") {
        return ErrorCode::kBadGateway;
    }
    if (value == "RATE_LIMITED") {
        return ErrorCode::kRateLimited;
    }
    if (value == "REQUEST_CONTEXT_INVALID") {
        return ErrorCode::kRequestContextInvalid;
    }
    if (value == "MESSAGE_NOT_SUPPORTED") {
        return ErrorCode::kMessageNotSupported;
    }
    if (value == "TRUSTED_GATEWAY_INVALID") {
        return ErrorCode::kTrustedGatewayInvalid;
    }
    if (value == "UPSTREAM_RESPONSE_INVALID") {
        return ErrorCode::kUpstreamResponseInvalid;
    }

    return std::nullopt;
}

DemoOptions ParseOptions(int argc, char* argv[]) {
    DemoOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--config-profile" && index + 1 < argc) {
            options.config_profile = argv[++index];
            if (options.config_profile == "demo") {
                options.gate_config = "configs/demo/gateway_client.conf";
                options.api_config = "configs/demo/api_gateway_client.conf";
                options.auth_config = "configs/demo/auth_server.conf";
                options.player_query_config = "configs/demo/player_query_server.conf";
                options.dungeon_config = "configs/demo/dungeon_runtime_server.conf";
            } else if (options.config_profile == "delivery") {
                options.gate_config = "configs/delivery/gateway_client.conf";
                options.api_config = "configs/delivery/api_gateway_client.conf";
                options.auth_config = "configs/delivery/auth_server.conf";
                options.player_query_config = "configs/delivery/player_query_server.conf";
                options.dungeon_config = "configs/delivery/dungeon_runtime_server.conf";
            }
        } else if (arg == "--gateway-config" && index + 1 < argc) {
            options.gate_config = argv[++index];
        } else if (arg == "--api-config" && index + 1 < argc) {
            options.api_config = argv[++index];
        } else if (arg == "--login-config" && index + 1 < argc) {
            options.auth_config = argv[++index];
        } else if ((arg == "--player-config" || arg == "--game-config") && index + 1 < argc) {
            options.player_query_config = argv[++index];
        } else if (arg == "--dungeon-config" && index + 1 < argc) {
            options.dungeon_config = argv[++index];
        } else if (arg == "--account" && index + 1 < argc) {
            options.account_name = argv[++index];
        } else if (arg == "--password" && index + 1 < argc) {
            options.password = argv[++index];
        } else if (arg == "--no-reset") {
            options.reset_demo_state = false;
        } else if (arg == "--happy-path-only") {
            options.run_negative_cases = false;
        } else if (arg == "--skip-session-recovery") {
            options.verify_session_recovery = false;
        } else if (arg == "--scenario" && index + 1 < argc) {
            const std::string value = argv[++index];
            if (value == "full") {
                options.scenario = DemoScenario::kFull;
            } else if (value == "login-only") {
                options.scenario = DemoScenario::kLoginOnly;
                options.run_negative_cases = false;
                options.verify_session_recovery = false;
            } else if (value == "load-only") {
                options.scenario = DemoScenario::kLoadOnly;
                options.run_negative_cases = false;
                options.verify_session_recovery = false;
                options.reset_demo_state = false;
            }
        } else if (arg == "--session-id" && index + 1 < argc) {
            options.session_id = argv[++index];
        } else if (arg == "--player-id" && index + 1 < argc) {
            options.player_id = std::stoll(argv[++index]);
        } else if (arg == "--expect-load-error" && index + 1 < argc) {
            options.expected_load_error = ParseErrorCode(argv[++index]);
        } else if (arg == "--expect-enter-error" && index + 1 < argc) {
            options.expected_enter_error = ParseErrorCode(argv[++index]);
        }
    }

    return options;
}

bool LoadConfig(const std::string& path, common::config::SimpleConfig& config) {
    if (config.LoadFromFile(path)) {
        return true;
    }

    auto entry = NewToolEvent("demo_client_config_load_failed");
    entry.Add("config_path", path);
    entry.Add("message", "failed to load config file");
    EmitToolLog(common::log::LogLevel::kError, entry);
    return false;
}

std::string FormatError(common::error::ErrorCode code, const std::string& message) {
    std::ostringstream output;
    output << "error_code=" << common::error::ToString(code);
    if (!message.empty()) {
        output << ", message=" << message;
    }
    return output.str();
}

bool Require(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    auto entry = NewToolEvent("demo_client_assertion_failed");
    entry.Add("message", message);
    EmitToolLog(common::log::LogLevel::kError, entry);
    return false;
}

bool RequireExpectedError(const CallResult& result,
                          common::error::ErrorCode expected_error,
                          const std::string& label) {
    if (result.ok) {
        auto entry = NewToolEvent("demo_client_expected_error_missing");
        entry.Add("operation", label);
        entry.Add("expected_error_code", std::string(common::error::ToString(expected_error)));
        EmitToolLog(common::log::LogLevel::kError, entry);
        return false;
    }

    if (result.error_code != expected_error) {
        auto entry = NewToolEvent("demo_client_expected_error_mismatch");
        entry.Add("operation", label);
        entry.Add("error_code", std::string(common::error::ToString(result.error_code)));
        entry.Add("expected_error_code", std::string(common::error::ToString(expected_error)));
        entry.Add("message", result.error_message);
        EmitToolLog(common::log::LogLevel::kError, entry);
        return false;
    }

    auto entry = NewToolEvent("demo_client_expected_error_observed");
    entry.Add("operation", label);
    entry.Add("error_code", std::string(common::error::ToString(expected_error)));
    EmitToolLog(common::log::LogLevel::kInfo, entry);
    return true;
}

void LogRewards(const google::protobuf::RepeatedPtrField<game_backend::proto::Reward>& rewards) {
    std::ostringstream output;
    bool first = true;
    for (const auto& reward : rewards) {
        if (!first) {
            output << ';';
        }
        output << reward.reward_type() << ':' << reward.amount();
        first = false;
    }
    auto entry = NewToolEvent("demo_client_rewards_observed");
    entry.Add("rewards", output.str());
    EmitToolLog(common::log::LogLevel::kInfo, entry);
}

common::net::RequestContext BuildContext(std::uint64_t request_id,
                                         const std::string& session_id = "",
                                         std::int64_t player_id = 0) {
    common::net::RequestContext context;
    context.trace_id = "demo-client-" + std::to_string(request_id);
    context.request_id = request_id;
    context.auth_token = session_id;
    context.player_id = player_id;
    return context;
}

bool ConnectMySql(const char* dependency, common::mysql::MySqlClient& client) {
    std::string error_message;
    if (client.Connect(&error_message)) {
        return true;
    }

    auto entry = NewToolEvent("demo_client_dependency_connect_failed");
    entry.Add("dependency", dependency);
    entry.Add("message", error_message);
    EmitToolLog(common::log::LogLevel::kError, entry);
    return false;
}

bool ConnectRedis(const char* dependency, common::redis::RedisClient& client) {
    std::string error_message;
    if (client.Connect(&error_message)) {
        return true;
    }

    auto entry = NewToolEvent("demo_client_dependency_connect_failed");
    entry.Add("dependency", dependency);
    entry.Add("message", error_message);
    EmitToolLog(common::log::LogLevel::kError, entry);
    return false;
}

template <typename RequestT>
CallResult SendGateMessage(framework::transport::TransportClient& client,
                           common::net::MessageId message_id,
                           const common::net::RequestContext& context,
                           RequestT* request) {
    const auto result = tools::client::SendGateProto(client, message_id, context, request);
    return {result.ok, result.error_code, result.error_message, result.packet};
}

template <typename RequestT>
CallResult SendHttpMessage(const tools::client::HttpClientProfile& api_profile,
                           std::string_view target,
                           common::net::MessageId message_id,
                           const common::net::RequestContext& context,
                           const std::string& auth_token,
                           RequestT* request) {
    const auto result = tools::client::SendHttpProto(api_profile, target, message_id, context, auth_token, request);
    return {result.ok, result.error_code, result.error_message, result.packet};
}

bool RequireGateBind(framework::transport::TransportClient& gate_client,
                     std::uint64_t request_id,
                     const std::string& auth_token,
                     bool relink,
                     const std::string& failure_message) {
    if (relink) {
        game_backend::proto::GateRelinkRequest request;
        request.set_auth_token(auth_token);
        const auto result = SendGateMessage(
            gate_client, common::net::MessageId::kGateRelinkRequest, BuildContext(request_id, auth_token), &request);
        if (!Require(result.ok, failure_message + ": " + FormatError(result.error_code, result.error_message))) {
            return false;
        }
        game_backend::proto::GateRelinkResponse response;
        return Require(common::net::ParseMessage(result.packet.body, &response) && response.success(),
                       "failed to parse gate relink response");
    }

    game_backend::proto::GateLoginRequest request;
    request.set_auth_token(auth_token);
    const auto result = SendGateMessage(
        gate_client, common::net::MessageId::kGateLoginRequest, BuildContext(request_id, auth_token), &request);
    if (!Require(result.ok, failure_message + ": " + FormatError(result.error_code, result.error_message))) {
        return false;
    }
    game_backend::proto::GateLoginResponse response;
    return Require(common::net::ParseMessage(result.packet.body, &response) && response.success(),
                   "failed to parse gate login response");
}

}  // namespace

int main(int argc, char* argv[]) {
    common::log::Logger::Instance().SetServiceName("demo_client");

    const auto options = ParseOptions(argc, argv);

    common::config::SimpleConfig gate_config;
    common::config::SimpleConfig api_config;
    common::config::SimpleConfig auth_config;
    common::config::SimpleConfig player_query_config;
    common::config::SimpleConfig dungeon_config;
    if (!LoadConfig(options.gate_config, gate_config) ||
        !LoadConfig(options.api_config, api_config) ||
        !LoadConfig(options.auth_config, auth_config) ||
        !LoadConfig(options.player_query_config, player_query_config) ||
        !LoadConfig(options.dungeon_config, dungeon_config)) {
        return 1;
    }

    const auto demo_data = demo::support::ReadDemoDataConfig(auth_config, player_query_config, dungeon_config);
    if (options.reset_demo_state) {
        common::mysql::MySqlClient account_mysql(
            common::mysql::ReadConnectionOptions(auth_config, "storage.account.mysql."));
        common::mysql::MySqlClient player_mysql(
            common::mysql::ReadConnectionOptions(player_query_config, "storage.player.mysql."));
        common::mysql::MySqlClient dungeon_mysql(
            common::mysql::ReadConnectionOptions(dungeon_config, "storage.battle.mysql."));
        common::redis::RedisClient account_redis(
            common::redis::ReadConnectionOptionsWithFallback(
                auth_config, "storage.session.redis.", "storage.account.redis."));
        common::redis::RedisClient player_redis(
            common::redis::ReadConnectionOptionsWithFallback(
                player_query_config, "storage.player_cache.redis.", "storage.player.redis."));
        common::redis::RedisClient dungeon_redis(
            common::redis::ReadConnectionOptionsWithFallback(
                dungeon_config, "storage.runtime.redis.", "storage.battle.redis."));
        if (!ConnectMySql("account_mysql", account_mysql) ||
            !ConnectMySql("player_mysql", player_mysql) ||
            !ConnectMySql("dungeon_mysql", dungeon_mysql) ||
            !ConnectRedis("account_redis", account_redis) ||
            !ConnectRedis("player_redis", player_redis) ||
            !ConnectRedis("dungeon_redis", dungeon_redis)) {
            return 1;
        }

        std::string error_message;
        if (!demo::support::EnsureDemoData(account_mysql, player_mysql, demo_data, &error_message) ||
            !demo::support::ResetDemoState(
                player_mysql, dungeon_mysql, account_redis, player_redis, dungeon_redis, demo_data, &error_message)) {
            auto entry = NewToolEvent("demo_client_state_prepare_failed");
            entry.Add("message", error_message);
            EmitToolLog(common::log::LogLevel::kError, entry);
            return 1;
        }
    }

    const auto gate_profile = tools::client::BuildTcpClientProfile(gate_config);
    const auto api_profile = tools::client::BuildHttpClientProfile(api_config);

    framework::transport::TransportClient primary_gate_client(
        gate_profile.host, gate_profile.port, gate_profile.timeout_ms, gate_profile.tls);
    framework::transport::TransportClient recovery_gate_client(
        gate_profile.host, gate_profile.port, gate_profile.timeout_ms, gate_profile.tls);

    std::uint64_t request_id = 1;

    if (options.scenario == DemoScenario::kLoadOnly) {
        if (!Require(!options.session_id.empty(), "load-only scenario requires --session-id") ||
            !Require(options.player_id != 0, "load-only scenario requires --player-id")) {
            return 1;
        }

        const auto gate_bind_ok = RequireGateBind(
            primary_gate_client, request_id++, options.session_id, true, "gate relink failed in load-only scenario");
        if (!gate_bind_ok) {
            if (options.expected_load_error.has_value()) {
                CallResult bind_error;
                bind_error.error_code = common::error::ErrorCode::kSessionInvalid;
                bind_error.error_message = "gate relink failed";
                return RequireExpectedError(bind_error, *options.expected_load_error, "load-only gate relink") ? 0 : 1;
            }
            return 1;
        }

        game_backend::proto::LoadPlayerRequest load_request;
        load_request.set_player_id(options.player_id);
        const auto load_call = SendHttpMessage(api_profile,
                                               "/api/v1/player/init",
                                               common::net::MessageId::kPlayerInitRequest,
                                               BuildContext(request_id++, options.session_id, options.player_id),
                                               options.session_id,
                                               &load_request);
        if (options.expected_load_error.has_value()) {
            return RequireExpectedError(load_call, *options.expected_load_error, "load-only request") ? 0 : 1;
        }
        if (!Require(load_call.ok, "load-only request failed: " + FormatError(load_call.error_code, load_call.error_message))) {
            return 1;
        }

        game_backend::proto::LoadPlayerResponse load_response;
        if (!Require(common::net::ParseMessage(load_call.packet.body, &load_response),
                     "failed to parse load-only response")) {
            return 1;
        }
        auto entry = NewToolEvent("demo_client_load_only_succeeded");
        entry.Add("player_id", load_response.home_init().player_display().player_id());
        entry.Add("cache", load_response.loaded_from_cache() ? "hit" : "miss");
        EmitToolLog(common::log::LogLevel::kInfo, entry);
        return 0;
    }

    game_backend::proto::LoginRequest login_request;
    login_request.set_account_name(options.account_name);
    login_request.set_password(options.password);
    const auto login_call = SendHttpMessage(
        api_profile, "/api/v1/auth/login", common::net::MessageId::kAuthLoginRequest, BuildContext(request_id++), "", &login_request);
    if (!Require(login_call.ok, "auth login failed: " + FormatError(login_call.error_code, login_call.error_message))) {
        return 1;
    }

    game_backend::proto::LoginResponse login_response;
    if (!Require(common::net::ParseMessage(login_call.packet.body, &login_response), "failed to parse login response")) {
        return 1;
    }

    auto login_entry = NewToolEvent("demo_client_login_succeeded");
    login_entry.Add("account_name", options.account_name);
    login_entry.Add("auth_token", framework::observability::MaskAuthToken(login_response.auth_token()));
    login_entry.Add("account_id", login_response.account_id());
    login_entry.Add("player_id", login_response.player_id());
    EmitToolLog(common::log::LogLevel::kInfo, login_entry);

    if (!RequireGateBind(primary_gate_client,
                         request_id++,
                         login_response.auth_token(),
                         false,
                         "gate login failed after auth login")) {
        return 1;
    }

    if (options.scenario == DemoScenario::kLoginOnly) {
        std::cout << "SESSION_ID=" << login_response.auth_token() << '\n';
        std::cout << "ACCOUNT_ID=" << login_response.account_id() << '\n';
        std::cout << "PLAYER_ID=" << login_response.player_id() << '\n';
        return 0;
    }

    game_backend::proto::LoadPlayerRequest load_request;
    load_request.set_player_id(login_response.player_id());
    if (options.verify_session_recovery) {
        if (!RequireGateBind(recovery_gate_client,
                             request_id++,
                             login_response.auth_token(),
                             true,
                             "gate relink failed during session recovery")) {
            return 1;
        }

        auto recovery_entry = NewToolEvent("demo_client_session_recovery_succeeded");
        recovery_entry.Add("player_id", login_response.player_id());
        recovery_entry.Add("mode", "gate_relink");
        EmitToolLog(common::log::LogLevel::kInfo, recovery_entry);
        primary_gate_client.Close();
    }

    const auto load_call = SendHttpMessage(api_profile,
                                           "/api/v1/player/init",
                                           common::net::MessageId::kPlayerInitRequest,
                                           BuildContext(request_id++, login_response.auth_token(), login_response.player_id()),
                                           login_response.auth_token(),
                                           &load_request);
    if (!Require(load_call.ok, "load player failed: " + FormatError(load_call.error_code, load_call.error_message))) {
        return 1;
    }

    game_backend::proto::LoadPlayerResponse load_response;
    if (!Require(common::net::ParseMessage(load_call.packet.body, &load_response), "failed to parse load player response")) {
        return 1;
    }

    const auto initial_stamina = load_response.home_init().resource_summary().stamina();
    auto player_entry = NewToolEvent("demo_client_load_player_succeeded");
    player_entry.Add("cache", load_response.loaded_from_cache() ? "hit" : "miss");
    player_entry.Add("player_id", load_response.home_init().player_display().player_id());
    player_entry.Add("nickname", load_response.home_init().player_display().nickname());
    player_entry.Add("level", static_cast<std::int64_t>(load_response.home_init().player_display().level()));
    player_entry.Add("stamina", static_cast<std::int64_t>(initial_stamina));
    player_entry.Add("main_stage_id",
                     static_cast<std::int64_t>(load_response.home_init().dungeon_summary().main_stage_id()));
    player_entry.Add("fight_power", load_response.home_init().character_summary().fight_power());
    player_entry.Add("gold", load_response.home_init().resource_summary().gold());
    player_entry.Add("diamond", load_response.home_init().resource_summary().diamond());
    EmitToolLog(common::log::LogLevel::kInfo, player_entry);

    game_backend::proto::LoadPlayerRequest second_load_request;
    second_load_request.set_player_id(login_response.player_id());
    const auto second_load_call = SendHttpMessage(api_profile,
                                                  "/api/v1/player/init",
                                                  common::net::MessageId::kPlayerInitRequest,
                                                  BuildContext(
                                                      request_id++, login_response.auth_token(), login_response.player_id()),
                                                  login_response.auth_token(),
                                                  &second_load_request);
    if (!Require(second_load_call.ok,
                 "second load failed: " + FormatError(second_load_call.error_code, second_load_call.error_message))) {
        return 1;
    }

    game_backend::proto::LoadPlayerResponse second_load_response;
    if (!Require(common::net::ParseMessage(second_load_call.packet.body, &second_load_response),
                 "failed to parse second load response") ||
        !Require(second_load_response.loaded_from_cache(), "second load should hit cache")) {
        return 1;
    }

    game_backend::proto::EnterDungeonRequest enter_request;
    enter_request.set_player_id(login_response.player_id());
    enter_request.set_stage_id(demo_data.stage_id);
    enter_request.set_mode("pve");
    const auto enter_request_id = request_id++;
    const auto enter_call = SendHttpMessage(api_profile,
                                            "/api/v1/dungeon/enter",
                                            common::net::MessageId::kEnterDungeonRequest,
                                            BuildContext(
                                                enter_request_id, login_response.auth_token(), login_response.player_id()),
                                            login_response.auth_token(),
                                            &enter_request);
    if (options.expected_enter_error.has_value()) {
        return RequireExpectedError(enter_call, *options.expected_enter_error, "enter dungeon request") ? 0 : 1;
    }

    game_backend::proto::EnterDungeonResponse enter_response;
    if (enter_call.ok) {
        if (!Require(common::net::ParseMessage(enter_call.packet.body, &enter_response),
                     "failed to parse enter dungeon response")) {
            return 1;
        }
    } else {
        const bool should_recover = enter_call.error_code == common::error::ErrorCode::kUpstreamTimeout ||
                                    enter_call.error_code == common::error::ErrorCode::kServiceUnavailable;
        if (!Require(should_recover,
                     "enter dungeon failed: " + FormatError(enter_call.error_code, enter_call.error_message))) {
            return 1;
        }

        auto recover_entry = NewToolEvent("demo_client_enter_dungeon_recovering");
        recover_entry.Add("request_id", static_cast<std::int64_t>(enter_request_id));
        recover_entry.Add("error_code", std::string(common::error::ToString(enter_call.error_code)));
        EmitToolLog(common::log::LogLevel::kWarn, recover_entry);

        game_backend::proto::GetActiveDungeonRequest recover_request;
        recover_request.set_player_id(login_response.player_id());
        const auto recover_call = SendHttpMessage(api_profile,
                                                  "/api/v1/dungeon/active",
                                                  common::net::MessageId::kGetActiveDungeonRequest,
                                                  BuildContext(
                                                      request_id++, login_response.auth_token(), login_response.player_id()),
                                                  login_response.auth_token(),
                                                  &recover_request);
        if (!Require(recover_call.ok,
                     "get active dungeon during recovery failed: " +
                         FormatError(recover_call.error_code, recover_call.error_message))) {
            return 1;
        }

        game_backend::proto::GetActiveDungeonResponse recover_response;
        if (!Require(common::net::ParseMessage(recover_call.packet.body, &recover_response),
                     "failed to parse recovery get active dungeon response")) {
            return 1;
        }

        if (recover_response.found()) {
            enter_response.set_session_id(recover_response.session_id());
            enter_response.set_remain_stamina(recover_response.remain_stamina());
            enter_response.set_seed(recover_response.seed());
            enter_response.set_settle_token(recover_response.settle_token());

            auto recovered_entry = NewToolEvent("demo_client_enter_dungeon_recovered");
            recovered_entry.Add("strategy", "get_active_dungeon");
            recovered_entry.Add("session_id", recover_response.session_id());
            EmitToolLog(common::log::LogLevel::kInfo, recovered_entry);
        } else {
            const auto retry_enter_call = SendHttpMessage(api_profile,
                                                          "/api/v1/dungeon/enter",
                                                          common::net::MessageId::kEnterDungeonRequest,
                                                          BuildContext(enter_request_id,
                                                                       login_response.auth_token(),
                                                                       login_response.player_id()),
                                                          login_response.auth_token(),
                                                          &enter_request);
            if (!Require(retry_enter_call.ok,
                         "enter dungeon retry failed: " +
                             FormatError(retry_enter_call.error_code, retry_enter_call.error_message))) {
                return 1;
            }
            if (!Require(common::net::ParseMessage(retry_enter_call.packet.body, &enter_response),
                         "failed to parse enter dungeon retry response")) {
                return 1;
            }

            auto recovered_entry = NewToolEvent("demo_client_enter_dungeon_recovered");
            recovered_entry.Add("strategy", "retry_same_request_id");
            recovered_entry.Add("session_id", enter_response.session_id());
            EmitToolLog(common::log::LogLevel::kInfo, recovered_entry);
        }
    }

    auto enter_entry = NewToolEvent("demo_client_enter_dungeon_succeeded");
    enter_entry.Add("player_id", login_response.player_id());
    enter_entry.Add("stage_id", static_cast<std::int64_t>(demo_data.stage_id));
    enter_entry.Add("session_id", enter_response.session_id());
    enter_entry.Add("remain_stamina", static_cast<std::int64_t>(enter_response.remain_stamina()));
    EmitToolLog(common::log::LogLevel::kInfo, enter_entry);

    game_backend::proto::GetActiveDungeonRequest active_dungeon_request;
    active_dungeon_request.set_player_id(login_response.player_id());
    const auto active_dungeon_call = SendHttpMessage(api_profile,
                                                    "/api/v1/dungeon/active",
                                                    common::net::MessageId::kGetActiveDungeonRequest,
                                                    BuildContext(
                                                        request_id++, login_response.auth_token(), login_response.player_id()),
                                                    login_response.auth_token(),
                                                    &active_dungeon_request);
    if (!Require(active_dungeon_call.ok,
                 "get active dungeon failed: " +
                     FormatError(active_dungeon_call.error_code, active_dungeon_call.error_message))) {
        return 1;
    }

    game_backend::proto::GetActiveDungeonResponse active_dungeon_response;
    if (!Require(common::net::ParseMessage(active_dungeon_call.packet.body, &active_dungeon_response),
                 "failed to parse get active dungeon response") ||
        !Require(active_dungeon_response.found(), "get active dungeon should return current active session") ||
        !Require(active_dungeon_response.session_id() == enter_response.session_id(),
                 "get active dungeon should return the same session_id as enter dungeon")) {
        return 1;
    }

    game_backend::proto::SettleDungeonRequest settle_request;
    settle_request.set_player_id(login_response.player_id());
    settle_request.set_session_id(enter_response.session_id());
    settle_request.set_stage_id(demo_data.stage_id);
    settle_request.set_star(3);
    settle_request.set_result_code(1);
    settle_request.set_client_score(123456);
    settle_request.set_settle_token(enter_response.settle_token());
    const auto settle_call = SendHttpMessage(api_profile,
                                             "/api/v1/dungeon/settle",
                                             common::net::MessageId::kSettleDungeonRequest,
                                             BuildContext(
                                                 request_id++, login_response.auth_token(), login_response.player_id()),
                                             login_response.auth_token(),
                                             &settle_request);
    if (!Require(settle_call.ok,
                 "settle dungeon failed: " + FormatError(settle_call.error_code, settle_call.error_message))) {
        return 1;
    }

    game_backend::proto::SettleDungeonResponse settle_response;
    if (!Require(common::net::ParseMessage(settle_call.packet.body, &settle_response), "failed to parse settle response")) {
        return 1;
    }

    auto settle_entry = NewToolEvent("demo_client_settle_dungeon_succeeded");
    settle_entry.Add("player_id", login_response.player_id());
    settle_entry.Add("stage_id", static_cast<std::int64_t>(demo_data.stage_id));
    settle_entry.Add("session_id", enter_response.session_id());
    settle_entry.Add("reward_grant_id", settle_response.reward_grant_id());
    settle_entry.Add("grant_status", static_cast<std::int64_t>(settle_response.grant_status()));
    EmitToolLog(common::log::LogLevel::kInfo, settle_entry);
    LogRewards(settle_response.reward_preview());

    if (options.run_negative_cases) {
        game_backend::proto::GetSettlementGrantStatusRequest grant_request;
        grant_request.set_player_id(login_response.player_id());
        grant_request.set_reward_grant_id(settle_response.reward_grant_id());
        const auto grant_call = SendHttpMessage(api_profile,
                                                "/api/v1/dungeon/grant-status",
                                                common::net::MessageId::kGetSettlementGrantStatusRequest,
                                                BuildContext(
                                                    request_id++, login_response.auth_token(), login_response.player_id()),
                                                login_response.auth_token(),
                                                &grant_request);
        if (!Require(grant_call.ok, "get settlement grant status failed: " +
                                        FormatError(grant_call.error_code, grant_call.error_message))) {
            return 1;
        }

        game_backend::proto::GetSettlementGrantStatusResponse grant_response;
        if (!Require(common::net::ParseMessage(grant_call.packet.body, &grant_response),
                     "failed to parse settlement grant status response") ||
            !Require(grant_response.grant_status() == 1, "settlement grant should be completed synchronously")) {
            return 1;
        }

        auto grant_entry = NewToolEvent("demo_client_settlement_grant_status_observed");
        grant_entry.Add("reward_grant_id", grant_response.reward_grant_id());
        grant_entry.Add("grant_status", static_cast<std::int64_t>(grant_response.grant_status()));
        EmitToolLog(common::log::LogLevel::kInfo, grant_entry);
    }

    game_backend::proto::LoadPlayerRequest final_load_request;
    final_load_request.set_player_id(login_response.player_id());
    const auto final_load_call = SendHttpMessage(api_profile,
                                                 "/api/v1/player/init",
                                                 common::net::MessageId::kPlayerInitRequest,
                                                 BuildContext(
                                                     request_id++, login_response.auth_token(), login_response.player_id()),
                                                 login_response.auth_token(),
                                                 &final_load_request);
    if (!Require(final_load_call.ok,
                 "reload player failed: " + FormatError(final_load_call.error_code, final_load_call.error_message))) {
        return 1;
    }

    game_backend::proto::LoadPlayerResponse final_load_response;
    if (!Require(common::net::ParseMessage(final_load_call.packet.body, &final_load_response),
                 "failed to parse reload player response") ||
        !Require(settle_response.grant_status() == 1, "settle dungeon should complete reward synchronously") ||
        !Require(final_load_response.home_init().resource_summary().stamina() == enter_response.remain_stamina(),
                 "reload player should reflect stamina consumption") ||
        !Require(final_load_response.home_init().resource_summary().gold() ==
                     load_response.home_init().resource_summary().gold() +
                         dungeon_config.GetInt("demo.stage_normal_gold_reward", 100),
                 "reload player should reflect gold reward") ||
        !Require(final_load_response.home_init().resource_summary().diamond() ==
                     load_response.home_init().resource_summary().diamond(),
                 "reload player should not mint diamond from client-reported star")) {
        return 1;
    }

    auto reload_entry = NewToolEvent("demo_client_reload_player_succeeded");
    reload_entry.Add("initial_stamina", static_cast<std::int64_t>(initial_stamina));
    reload_entry.Add("current_stamina",
                     static_cast<std::int64_t>(final_load_response.home_init().resource_summary().stamina()));
    reload_entry.Add("gold", final_load_response.home_init().resource_summary().gold());
    reload_entry.Add("diamond", final_load_response.home_init().resource_summary().diamond());
    EmitToolLog(common::log::LogLevel::kInfo, reload_entry);

    if (options.run_negative_cases) {
        game_backend::proto::LoginRequest bad_login_request;
        bad_login_request.set_account_name(options.account_name);
        bad_login_request.set_password("bad-password");
        const auto bad_login_call = SendHttpMessage(api_profile,
                                                    "/api/v1/auth/login",
                                                    common::net::MessageId::kAuthLoginRequest,
                                                    BuildContext(request_id++),
                                                    "",
                                                    &bad_login_request);
        if (!RequireExpectedError(bad_login_call, common::error::ErrorCode::kInvalidPassword, "invalid password")) {
            return 1;
        }

        const auto duplicate_settle_call = SendHttpMessage(api_profile,
                                                           "/api/v1/dungeon/settle",
                                                           common::net::MessageId::kSettleDungeonRequest,
                                                           BuildContext(request_id++,
                                                                        login_response.auth_token(),
                                                                        login_response.player_id()),
                                                           login_response.auth_token(),
                                                           &settle_request);
        if (!Require(duplicate_settle_call.ok,
                     "duplicate settle failed: " +
                         FormatError(duplicate_settle_call.error_code, duplicate_settle_call.error_message))) {
            return 1;
        }
        game_backend::proto::SettleDungeonResponse duplicate_settle_response;
        if (!Require(common::net::ParseMessage(duplicate_settle_call.packet.body, &duplicate_settle_response),
                     "failed to parse duplicate settle response") ||
            !Require(duplicate_settle_response.reward_grant_id() == settle_response.reward_grant_id(),
                     "duplicate settle should replay the same reward grant")) {
            return 1;
        }

        game_backend::proto::EnterDungeonRequest invalid_star_enter_request;
        invalid_star_enter_request.set_player_id(login_response.player_id());
        invalid_star_enter_request.set_stage_id(demo_data.stage_id);
        invalid_star_enter_request.set_mode("pve");
        const auto invalid_star_enter_call = SendHttpMessage(api_profile,
                                                             "/api/v1/dungeon/enter",
                                                             common::net::MessageId::kEnterDungeonRequest,
                                                             BuildContext(request_id++,
                                                                          login_response.auth_token(),
                                                                          login_response.player_id()),
                                                             login_response.auth_token(),
                                                             &invalid_star_enter_request);
        if (!Require(invalid_star_enter_call.ok,
                     "second enter dungeon failed: " +
                         FormatError(invalid_star_enter_call.error_code, invalid_star_enter_call.error_message))) {
            return 1;
        }

        game_backend::proto::EnterDungeonResponse invalid_star_enter_response;
        if (!Require(common::net::ParseMessage(invalid_star_enter_call.packet.body, &invalid_star_enter_response),
                     "failed to parse second enter dungeon response")) {
            return 1;
        }

        game_backend::proto::SettleDungeonRequest invalid_star_request;
        invalid_star_request.set_player_id(login_response.player_id());
        invalid_star_request.set_session_id(invalid_star_enter_response.session_id());
        invalid_star_request.set_stage_id(demo_data.stage_id);
        invalid_star_request.set_star(99);
        invalid_star_request.set_result_code(1);
        invalid_star_request.set_client_score(1);
        invalid_star_request.set_settle_token(invalid_star_enter_response.settle_token());
        const auto invalid_star_call = SendHttpMessage(api_profile,
                                                       "/api/v1/dungeon/settle",
                                                       common::net::MessageId::kSettleDungeonRequest,
                                                       BuildContext(request_id++,
                                                                    login_response.auth_token(),
                                                                    login_response.player_id()),
                                                       login_response.auth_token(),
                                                       &invalid_star_request);
        if (!RequireExpectedError(invalid_star_call, common::error::ErrorCode::kInvalidStar, "invalid star")) {
            return 1;
        }

        game_backend::proto::SettleDungeonRequest cleanup_settle_request;
        cleanup_settle_request.set_player_id(login_response.player_id());
        cleanup_settle_request.set_session_id(invalid_star_enter_response.session_id());
        cleanup_settle_request.set_stage_id(demo_data.stage_id);
        cleanup_settle_request.set_star(1);
        cleanup_settle_request.set_result_code(1);
        cleanup_settle_request.set_client_score(654321);
        cleanup_settle_request.set_settle_token(invalid_star_enter_response.settle_token());
        const auto cleanup_settle_call = SendHttpMessage(api_profile,
                                                         "/api/v1/dungeon/settle",
                                                         common::net::MessageId::kSettleDungeonRequest,
                                                         BuildContext(request_id++,
                                                                      login_response.auth_token(),
                                                                      login_response.player_id()),
                                                         login_response.auth_token(),
                                                         &cleanup_settle_request);
        if (!Require(cleanup_settle_call.ok,
                     "cleanup settle failed: " +
                         FormatError(cleanup_settle_call.error_code, cleanup_settle_call.error_message))) {
            return 1;
        }
    }

    auto done_entry = NewToolEvent("demo_client_completed");
    done_entry.Add("account_name", options.account_name);
    EmitToolLog(common::log::LogLevel::kInfo, done_entry);
    return 0;
}
