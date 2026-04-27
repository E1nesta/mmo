#include "runtime/foundation/config/simple_config.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/storage/mysql/mysql_client.h"
#include "runtime/storage/redis/redis_client.h"
#include "runtime/transport/tls_options.h"
#include "runtime/transport/transport_client.h"
#include "tools/client_channel_support.h"
#include "tools/demo_support.h"

#include "game_backend.pb.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class LoadScenario {
    kLoginOnly,
    kLoadOnly,
    kFullLoop,
    kMixed,
};

enum class SeedProfile {
    kUniform,
    kRealistic,
};

struct LoadOptions {
    std::string config_profile = "demo";
    std::string gate_config = "configs/demo/gateway_client.conf";
    std::string api_config = "configs/demo/api_gateway_client.conf";
    std::string auth_config = "configs/demo/auth_server.conf";
    std::string player_query_config = "configs/demo/player_query_server.conf";
    std::string dungeon_config = "configs/demo/dungeon_runtime_server.conf";
    LoadScenario scenario = LoadScenario::kLoadOnly;
    std::string account_prefix = "loadtest";
    std::string password = "demo123";
    std::string report_json_path;
    int concurrency = 4;
    int duration_seconds = 30;
    int ramp_up_seconds = 0;
    int account_count = 0;
    int think_time_ms = 0;
    bool reset_demo_state = true;
    SeedProfile seed_profile = SeedProfile::kUniform;
    int mixed_login_ratio = 20;
    int mixed_load_ratio = 40;
    int mixed_full_ratio = 40;
};

struct CallResult {
    bool ok = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    common::net::Packet packet;
};

struct UserContext {
    demo::support::DemoDataConfig demo_config;
    std::string session_id;
};

struct ConnectionProfile {
    tools::client::TcpClientProfile gate;
    tools::client::HttpClientProfile api;
};

struct AggregateStats {
    std::size_t total_requests = 0;
    std::size_t successful_requests = 0;
    std::size_t failed_requests = 0;
    std::size_t scenario_successes = 0;
    std::size_t scenario_failures = 0;
    double average_latency_ms = 0.0;
    double qps = 0.0;
    int p50_latency_ms = 0;
    int p95_latency_ms = 0;
    int p99_latency_ms = 0;
    std::unordered_map<std::string, std::size_t> error_counts;
    std::unordered_map<std::string, std::size_t> operation_counts;
};

std::string ScenarioToString(LoadScenario scenario) {
    switch (scenario) {
    case LoadScenario::kLoginOnly:
        return "login-only";
    case LoadScenario::kLoadOnly:
        return "load-only";
    case LoadScenario::kFullLoop:
        return "full-loop";
    case LoadScenario::kMixed:
        return "mixed";
    }
    return "unknown";
}

std::string SeedProfileToString(SeedProfile profile) {
    switch (profile) {
    case SeedProfile::kUniform:
        return "uniform";
    case SeedProfile::kRealistic:
        return "realistic";
    }
    return "unknown";
}

LoadOptions ParseOptions(int argc, char* argv[]) {
    LoadOptions options;

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
            } else if (options.config_profile == "local") {
                options.gate_config = "configs/local/gateway_client.conf";
                options.api_config = "configs/local/api_gateway_client.conf";
                options.auth_config = "configs/auth_server.conf";
                options.player_query_config = "configs/player_query_server.conf";
                options.dungeon_config = "configs/dungeon_runtime_server.conf";
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
        } else if (arg == "--scenario" && index + 1 < argc) {
            const std::string value = argv[++index];
            if (value == "login-only") {
                options.scenario = LoadScenario::kLoginOnly;
            } else if (value == "load-only") {
                options.scenario = LoadScenario::kLoadOnly;
            } else if (value == "full-loop") {
                options.scenario = LoadScenario::kFullLoop;
            } else if (value == "mixed") {
                options.scenario = LoadScenario::kMixed;
            }
        } else if (arg == "--seed-profile" && index + 1 < argc) {
            const std::string value = argv[++index];
            if (value == "uniform") {
                options.seed_profile = SeedProfile::kUniform;
            } else if (value == "realistic") {
                options.seed_profile = SeedProfile::kRealistic;
            }
        } else if (arg == "--concurrency" && index + 1 < argc) {
            options.concurrency = std::stoi(argv[++index]);
        } else if (arg == "--duration" && index + 1 < argc) {
            options.duration_seconds = std::stoi(argv[++index]);
        } else if (arg == "--ramp-up" && index + 1 < argc) {
            options.ramp_up_seconds = std::stoi(argv[++index]);
        } else if (arg == "--account-prefix" && index + 1 < argc) {
            options.account_prefix = argv[++index];
        } else if (arg == "--account-count" && index + 1 < argc) {
            options.account_count = std::stoi(argv[++index]);
        } else if (arg == "--password" && index + 1 < argc) {
            options.password = argv[++index];
        } else if (arg == "--think-time-ms" && index + 1 < argc) {
            options.think_time_ms = std::stoi(argv[++index]);
        } else if (arg == "--mixed-login-ratio" && index + 1 < argc) {
            options.mixed_login_ratio = std::stoi(argv[++index]);
        } else if (arg == "--mixed-load-ratio" && index + 1 < argc) {
            options.mixed_load_ratio = std::stoi(argv[++index]);
        } else if (arg == "--mixed-full-ratio" && index + 1 < argc) {
            options.mixed_full_ratio = std::stoi(argv[++index]);
        } else if (arg == "--report-json" && index + 1 < argc) {
            options.report_json_path = argv[++index];
        } else if (arg == "--no-reset") {
            options.reset_demo_state = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: load_client [options]\n"
                << "  --config-profile <demo|delivery|local>\n"
                << "  --scenario <login-only|load-only|full-loop|mixed>\n"
                << "  --seed-profile <uniform|realistic>\n"
                << "  --concurrency <n>\n"
                << "  --duration <seconds>\n"
                << "  --ramp-up <seconds>\n"
                << "  --account-prefix <prefix>\n"
                << "  --account-count <n>\n"
                << "  --password <value>\n"
                << "  --think-time-ms <ms>\n"
                << "  --mixed-login-ratio <n>\n"
                << "  --mixed-load-ratio <n>\n"
                << "  --mixed-full-ratio <n>\n"
                << "  --report-json <path>\n"
                << "  --no-reset\n";
            std::exit(0);
        }
    }

    return options;
}

bool LoadConfig(const std::string& path, common::config::SimpleConfig& config) {
    if (config.LoadFromFile(path)) {
        return true;
    }
    std::cerr << "failed to load config file: " << path << '\n';
    return false;
}

common::net::RequestContext BuildContext(std::uint64_t request_id,
                                         const std::string& session_id = {},
                                         std::int64_t player_id = 0) {
    common::net::RequestContext context;
    context.trace_id = "load-client-" + std::to_string(request_id);
    context.request_id = request_id;
    context.auth_token = session_id;
    context.player_id = player_id;
    return context;
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

bool ConnectMySql(const char* dependency, common::mysql::MySqlClient& client) {
    std::string error_message;
    if (client.Connect(&error_message)) {
        return true;
    }
    std::cerr << "failed to connect " << dependency << ": " << error_message << '\n';
    return false;
}

bool ConnectRedis(const char* dependency, common::redis::RedisClient& client) {
    std::string error_message;
    if (client.Connect(&error_message)) {
        return true;
    }
    std::cerr << "failed to connect " << dependency << ": " << error_message << '\n';
    return false;
}

std::uint64_t MixBits(std::uint64_t value) {
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
}

int DeterministicRange(std::uint64_t seed, int min_value, int max_value) {
    if (max_value <= min_value) {
        return min_value;
    }
    const auto width = static_cast<std::uint64_t>(max_value - min_value + 1);
    return min_value + static_cast<int>(MixBits(seed) % width);
}

std::int64_t DeterministicRange64(std::uint64_t seed, std::int64_t min_value, std::int64_t max_value) {
    if (max_value <= min_value) {
        return min_value;
    }
    const auto width = static_cast<std::uint64_t>(max_value - min_value + 1);
    return min_value + static_cast<std::int64_t>(MixBits(seed) % width);
}

std::vector<demo::support::DemoRoleConfig> BuildRolesForCohort(int level, int star_floor, int star_ceiling) {
    std::vector<demo::support::DemoRoleConfig> roles;
    roles.reserve(3);
    roles.push_back({1001, level, std::clamp(star_ceiling, 1, 5), std::max(0, star_ceiling - 2)});
    roles.push_back({1002, std::max(1, level - 2), std::clamp((star_floor + star_ceiling) / 2, 1, 5), 0});
    roles.push_back({1003, std::max(1, level - 4), std::clamp(star_floor, 1, 5), 0});
    return roles;
}

demo::support::DemoDataConfig BuildRealisticUserConfig(const demo::support::DemoDataConfig& base,
                                                       const std::string& account_prefix,
                                                       int index,
                                                       int account_count) {
    demo::support::DemoDataConfig config = base;
    config.account_id = base.account_id + index;
    config.player_id = base.player_id + index;
    config.account_name = account_prefix + std::to_string(index + 1);
    config.player_name = account_prefix + "_player_" + std::to_string(index + 1);

    const auto percentile = account_count <= 0 ? 0 : (index * 100) / account_count;
    const auto seed = static_cast<std::uint64_t>(config.player_id);
    if (percentile < 60) {
        config.level = DeterministicRange(seed + 11U, 4, 12);
        config.stamina = DeterministicRange(seed + 13U, 48, 120);
        config.gold = DeterministicRange64(seed + 17U, 300, 1800);
        config.diamond = DeterministicRange64(seed + 19U, 20, 150);
        config.main_stage_id = base.stage_id;
        config.fight_power = DeterministicRange64(seed + 23U, config.level * 90, config.level * 130);
        config.roles = BuildRolesForCohort(config.level, 1, 2);
    } else if (percentile < 90) {
        config.level = DeterministicRange(seed + 29U, 18, 30);
        config.stamina = DeterministicRange(seed + 31U, 72, 160);
        config.gold = DeterministicRange64(seed + 37U, 2000, 9000);
        config.diamond = DeterministicRange64(seed + 41U, 120, 500);
        config.main_stage_id = base.stage_id;
        config.fight_power = DeterministicRange64(seed + 43U, config.level * 160, config.level * 220);
        config.roles = BuildRolesForCohort(config.level, 2, 3);
    } else {
        config.level = DeterministicRange(seed + 47U, 35, 60);
        config.stamina = DeterministicRange(seed + 53U, 100, 220);
        config.gold = DeterministicRange64(seed + 59U, 10000, 50000);
        config.diamond = DeterministicRange64(seed + 61U, 400, 2500);
        config.main_stage_id = base.stage_id;
        config.fight_power = DeterministicRange64(seed + 67U, config.level * 260, config.level * 340);
        config.roles = BuildRolesForCohort(config.level, 3, 5);
    }

    return config;
}

demo::support::DemoDataConfig BuildUserConfig(const demo::support::DemoDataConfig& base,
                                              const std::string& account_prefix,
                                              int index,
                                              int account_count,
                                              SeedProfile seed_profile) {
    if (seed_profile == SeedProfile::kRealistic) {
        return BuildRealisticUserConfig(base, account_prefix, index, account_count);
    }

    demo::support::DemoDataConfig config = base;
    config.account_id = base.account_id + index;
    config.player_id = base.player_id + index;
    config.account_name = account_prefix + std::to_string(index + 1);
    config.player_name = account_prefix + "_player_" + std::to_string(index + 1);
    return config;
}

ConnectionProfile BuildConnectionProfile(const common::config::SimpleConfig& gate_config,
                                         const common::config::SimpleConfig& api_config) {
    ConnectionProfile profile;
    profile.gate = tools::client::BuildTcpClientProfile(gate_config);
    profile.api = tools::client::BuildHttpClientProfile(api_config);
    return profile;
}

class StatsCollector {
public:
    void RecordRequest(std::string_view operation,
                       bool success,
                       common::error::ErrorCode error_code,
                       std::int64_t latency_ms) {
        std::lock_guard lock(mutex_);
        ++total_requests_;
        if (success) {
            ++successful_requests_;
        } else {
            ++failed_requests_;
            ++error_counts_[std::string(common::error::ToString(error_code))];
        }
        ++operation_counts_[std::string(operation)];
        latencies_ms_.push_back(latency_ms);
    }

    void RecordScenario(bool success) {
        std::lock_guard lock(mutex_);
        if (success) {
            ++scenario_successes_;
        } else {
            ++scenario_failures_;
        }
    }

    AggregateStats Build(std::chrono::steady_clock::duration elapsed) const {
        std::lock_guard lock(mutex_);
        AggregateStats stats;
        stats.total_requests = total_requests_;
        stats.successful_requests = successful_requests_;
        stats.failed_requests = failed_requests_;
        stats.scenario_successes = scenario_successes_;
        stats.scenario_failures = scenario_failures_;
        stats.error_counts = error_counts_;
        stats.operation_counts = operation_counts_;

        if (!latencies_ms_.empty()) {
            const auto sorted = SortedLatencies();
            const std::int64_t sum =
                std::accumulate(latencies_ms_.begin(), latencies_ms_.end(), static_cast<std::int64_t>(0));
            stats.average_latency_ms = static_cast<double>(sum) / static_cast<double>(latencies_ms_.size());
            stats.p50_latency_ms = Percentile(sorted, 0.50);
            stats.p95_latency_ms = Percentile(sorted, 0.95);
            stats.p99_latency_ms = Percentile(sorted, 0.99);
        }

        const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
        if (elapsed_seconds > 0.0) {
            stats.qps = static_cast<double>(stats.total_requests) / elapsed_seconds;
        }
        return stats;
    }

private:
    static int Percentile(const std::vector<std::int64_t>& sorted_values, double percentile) {
        if (sorted_values.empty()) {
            return 0;
        }
        const auto index = static_cast<std::size_t>(
            std::clamp<std::size_t>(
                static_cast<std::size_t>(std::ceil(percentile * static_cast<double>(sorted_values.size()))) - 1U,
                0U,
                sorted_values.size() - 1U));
        return static_cast<int>(sorted_values[index]);
    }

    std::vector<std::int64_t> SortedLatencies() const {
        auto sorted = latencies_ms_;
        std::sort(sorted.begin(), sorted.end());
        return sorted;
    }

    mutable std::mutex mutex_;
    std::size_t total_requests_ = 0;
    std::size_t successful_requests_ = 0;
    std::size_t failed_requests_ = 0;
    std::size_t scenario_successes_ = 0;
    std::size_t scenario_failures_ = 0;
    std::unordered_map<std::string, std::size_t> error_counts_;
    std::unordered_map<std::string, std::size_t> operation_counts_;
    std::vector<std::int64_t> latencies_ms_;
};

class ResetContext {
public:
    ResetContext(const common::config::SimpleConfig& login_config,
                 const common::config::SimpleConfig& player_config,
                 const common::config::SimpleConfig& dungeon_config)
        : account_mysql_(common::mysql::ReadConnectionOptions(login_config, "storage.account.mysql.")),
          player_mysql_(common::mysql::ReadConnectionOptions(player_config, "storage.player.mysql.")),
          dungeon_mysql_(common::mysql::ReadConnectionOptions(dungeon_config, "storage.battle.mysql.")),
          account_redis_(common::redis::ReadConnectionOptionsWithFallback(
              login_config, "storage.session.redis.", "storage.account.redis.")),
          player_redis_(common::redis::ReadConnectionOptionsWithFallback(
              player_config, "storage.player_cache.redis.", "storage.player.redis.")),
          dungeon_redis_(common::redis::ReadConnectionOptionsWithFallback(
              dungeon_config, "storage.runtime.redis.", "storage.battle.redis.")) {}

    bool Initialize() {
        return ConnectMySql("account_mysql", account_mysql_) &&
               ConnectMySql("player_mysql", player_mysql_) &&
               ConnectMySql("dungeon_mysql", dungeon_mysql_) &&
               ConnectRedis("account_redis", account_redis_) &&
               ConnectRedis("player_redis", player_redis_) &&
               ConnectRedis("dungeon_redis", dungeon_redis_);
    }

    bool EnsureAndReset(const demo::support::DemoDataConfig& config, std::string* error_message) {
        return demo::support::EnsureDemoData(account_mysql_, player_mysql_, config, error_message) &&
               demo::support::ResetDemoState(
                   player_mysql_, dungeon_mysql_, account_redis_, player_redis_, dungeon_redis_, config, error_message);
    }

    bool ResetOnly(const demo::support::DemoDataConfig& config, std::string* error_message) {
        return demo::support::ResetDemoState(
            player_mysql_, dungeon_mysql_, account_redis_, player_redis_, dungeon_redis_, config, error_message);
    }

private:
    common::mysql::MySqlClient account_mysql_;
    common::mysql::MySqlClient player_mysql_;
    common::mysql::MySqlClient dungeon_mysql_;
    common::redis::RedisClient account_redis_;
    common::redis::RedisClient player_redis_;
    common::redis::RedisClient dungeon_redis_;
};

template <typename ResponseT, typename InvokeFn>
bool ExecuteTimedCall(std::string_view operation,
                      StatsCollector* stats,
                      ResponseT* response,
                      InvokeFn&& invoke,
                      CallResult* out_result = nullptr) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto result = invoke();
    const auto latency_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
    if (stats != nullptr) {
        stats->RecordRequest(operation, result.ok, result.error_code, latency_ms);
    }
    if (out_result != nullptr) {
        *out_result = result;
    }
    if (!result.ok) {
        return false;
    }
    if (response != nullptr && !common::net::ParseMessage(result.packet.body, response)) {
        if (stats != nullptr) {
            stats->RecordRequest(operation, false, common::error::ErrorCode::kBadGateway, latency_ms);
        }
        return false;
    }
    return true;
}

bool RunLoginOnlyIteration(const ConnectionProfile& connection_profile,
                           framework::transport::TransportClient& gate_client,
                           const UserContext& user,
                           const LoadOptions& options,
                           std::uint64_t* request_id,
                           StatsCollector* stats) {
    game_backend::proto::LoginRequest login_request;
    login_request.set_account_name(user.demo_config.account_name);
    login_request.set_password(options.password);
    game_backend::proto::LoginResponse login_response;
    if (!ExecuteTimedCall(
            "auth_login",
            stats,
            &login_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/auth/login",
                                       common::net::MessageId::kAuthLoginRequest,
                                       BuildContext((*request_id)++),
                                       "",
                                       &login_request);
            })) {
        return false;
    }

    gate_client.Close();
    CallResult gate_result;
    const bool gate_ok = ExecuteTimedCall(
        "gate_login",
        stats,
        static_cast<google::protobuf::MessageLite*>(nullptr),
        [&] {
            game_backend::proto::GateLoginRequest gate_request;
            gate_request.set_auth_token(login_response.auth_token());
            return SendGateMessage(
                gate_client,
                common::net::MessageId::kGateLoginRequest,
                BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                &gate_request);
        },
        &gate_result);
    return gate_ok;
}

bool RefreshSession(const ConnectionProfile& connection_profile,
                    framework::transport::TransportClient& gate_client,
                    const UserContext& user,
                    const LoadOptions& options,
                    std::uint64_t* request_id,
                    StatsCollector* stats,
                    std::string* session_id) {
    game_backend::proto::LoginRequest login_request;
    login_request.set_account_name(user.demo_config.account_name);
    login_request.set_password(options.password);
    game_backend::proto::LoginResponse login_response;
    if (!ExecuteTimedCall(
            "auth_login",
            stats,
            &login_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/auth/login",
                                       common::net::MessageId::kAuthLoginRequest,
                                       BuildContext((*request_id)++),
                                       "",
                                       &login_request);
            })) {
        return false;
    }
    *session_id = login_response.auth_token();

    gate_client.Close();
    CallResult gate_result;
    return ExecuteTimedCall(
        "gate_login",
        stats,
        static_cast<google::protobuf::MessageLite*>(nullptr),
        [&] {
            game_backend::proto::GateLoginRequest gate_request;
            gate_request.set_auth_token(*session_id);
            return SendGateMessage(gate_client,
                                   common::net::MessageId::kGateLoginRequest,
                                   BuildContext((*request_id)++, *session_id, user.demo_config.player_id),
                                   &gate_request);
        },
        &gate_result);
}

bool RunLoadOnlyIteration(const ConnectionProfile& connection_profile,
                          framework::transport::TransportClient& gate_client,
                          const UserContext& user,
                          const LoadOptions& options,
                          std::uint64_t* request_id,
                          StatsCollector* stats,
                          std::string* session_id) {
    if (session_id->empty() &&
        !RefreshSession(connection_profile, gate_client, user, options, request_id, stats, session_id)) {
        return false;
    }

    game_backend::proto::LoadPlayerRequest load_request;
    load_request.set_player_id(user.demo_config.player_id);
    game_backend::proto::LoadPlayerResponse load_response;
    CallResult result;
    if (ExecuteTimedCall(
            "player_init",
            stats,
            &load_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/player/init",
                                       common::net::MessageId::kPlayerInitRequest,
                                       BuildContext((*request_id)++, *session_id, user.demo_config.player_id),
                                       *session_id,
                                       &load_request);
            },
            &result)) {
        return true;
    }

    if (result.error_code == common::error::ErrorCode::kSessionInvalid) {
        session_id->clear();
        gate_client.Close();
    }
    return false;
}

bool RunFullLoopIteration(const ConnectionProfile& connection_profile,
                          framework::transport::TransportClient& gate_client,
                          const UserContext& user,
                          const LoadOptions& options,
                          std::uint64_t* request_id,
                          StatsCollector* stats) {
    game_backend::proto::LoginRequest login_request;
    login_request.set_account_name(user.demo_config.account_name);
    login_request.set_password(options.password);
    game_backend::proto::LoginResponse login_response;
    if (!ExecuteTimedCall(
            "auth_login",
            stats,
            &login_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/auth/login",
                                       common::net::MessageId::kAuthLoginRequest,
                                       BuildContext((*request_id)++),
                                       "",
                                       &login_request);
            })) {
        return false;
    }

    gate_client.Close();
    CallResult gate_result;
    if (!ExecuteTimedCall(
            "gate_login",
            stats,
            static_cast<google::protobuf::MessageLite*>(nullptr),
            [&] {
                game_backend::proto::GateLoginRequest gate_request;
                gate_request.set_auth_token(login_response.auth_token());
                return SendGateMessage(gate_client,
                                       common::net::MessageId::kGateLoginRequest,
                                       BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                                       &gate_request);
            },
            &gate_result)) {
        return false;
    }

    game_backend::proto::LoadPlayerRequest load_request;
    load_request.set_player_id(user.demo_config.player_id);
    game_backend::proto::LoadPlayerResponse load_response;
    if (!ExecuteTimedCall(
            "player_init",
            stats,
            &load_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/player/init",
                                       common::net::MessageId::kPlayerInitRequest,
                                       BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                                       login_response.auth_token(),
                                       &load_request);
            })) {
        return false;
    }

    game_backend::proto::EnterDungeonRequest enter_request;
    enter_request.set_player_id(user.demo_config.player_id);
    enter_request.set_stage_id(user.demo_config.stage_id);
    enter_request.set_mode("pve");
    game_backend::proto::EnterDungeonResponse enter_response;
    if (!ExecuteTimedCall(
            "dungeon_enter",
            stats,
            &enter_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/dungeon/enter",
                                       common::net::MessageId::kEnterDungeonRequest,
                                       BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                                       login_response.auth_token(),
                                       &enter_request);
            })) {
        return false;
    }

    game_backend::proto::SettleDungeonRequest settle_request;
    settle_request.set_player_id(user.demo_config.player_id);
    settle_request.set_session_id(enter_response.session_id());
    settle_request.set_stage_id(user.demo_config.stage_id);
    settle_request.set_star(3);
    settle_request.set_result_code(1);
    settle_request.set_client_score(123456);
    settle_request.set_settle_token(enter_response.settle_token());
    game_backend::proto::SettleDungeonResponse settle_response;
    if (!ExecuteTimedCall(
            "dungeon_settle",
            stats,
            &settle_response,
            [&] {
                return SendHttpMessage(connection_profile.api,
                                       "/api/v1/dungeon/settle",
                                       common::net::MessageId::kSettleDungeonRequest,
                                       BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                                       login_response.auth_token(),
                                       &settle_request);
            })) {
        return false;
    }

    game_backend::proto::LoadPlayerRequest final_load_request;
    final_load_request.set_player_id(user.demo_config.player_id);
    game_backend::proto::LoadPlayerResponse final_load_response;
    return ExecuteTimedCall(
        "player_reload",
        stats,
        &final_load_response,
        [&] {
            return SendHttpMessage(connection_profile.api,
                                   "/api/v1/player/init",
                                   common::net::MessageId::kPlayerInitRequest,
                                   BuildContext((*request_id)++, login_response.auth_token(), user.demo_config.player_id),
                                   login_response.auth_token(),
                                   &final_load_request);
        });
}

std::uint32_t NextRandom(std::uint32_t* state) {
    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}

LoadScenario SelectMixedScenario(const LoadOptions& options, std::uint32_t* random_state) {
    const auto total = options.mixed_login_ratio + options.mixed_load_ratio + options.mixed_full_ratio;
    if (total <= 0) {
        return LoadScenario::kLoadOnly;
    }

    const auto roll = static_cast<int>(NextRandom(random_state) % static_cast<std::uint32_t>(total));
    if (roll < options.mixed_login_ratio) {
        return LoadScenario::kLoginOnly;
    }
    if (roll < options.mixed_login_ratio + options.mixed_load_ratio) {
        return LoadScenario::kLoadOnly;
    }
    return LoadScenario::kFullLoop;
}

bool RunIterationForScenario(LoadScenario scenario,
                             const ConnectionProfile& connection_profile,
                             framework::transport::TransportClient& gate_client,
                             const UserContext& user,
                             const LoadOptions& options,
                             std::uint64_t* request_id,
                             StatsCollector* stats,
                             std::string* session_id) {
    switch (scenario) {
    case LoadScenario::kLoginOnly:
        return RunLoginOnlyIteration(connection_profile, gate_client, user, options, request_id, stats);
    case LoadScenario::kLoadOnly:
        return RunLoadOnlyIteration(connection_profile, gate_client, user, options, request_id, stats, session_id);
    case LoadScenario::kFullLoop:
        return RunFullLoopIteration(connection_profile, gate_client, user, options, request_id, stats);
    case LoadScenario::kMixed:
        break;
    }
    return false;
}

std::string EscapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 2U);
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

void WriteReportJson(const std::string& path,
                     const LoadOptions& options,
                     const AggregateStats& stats) {
    std::ofstream output(path);
    if (!output.is_open()) {
        std::cerr << "failed to open report path: " << path << '\n';
        return;
    }

    output << "{\n";
    output << "  \"scenario\": \"" << EscapeJson(ScenarioToString(options.scenario)) << "\",\n";
    output << "  \"seed_profile\": \"" << EscapeJson(SeedProfileToString(options.seed_profile)) << "\",\n";
    output << "  \"concurrency\": " << options.concurrency << ",\n";
    output << "  \"duration_seconds\": " << options.duration_seconds << ",\n";
    output << "  \"account_count\": " << options.account_count << ",\n";
    output << "  \"mixed_login_ratio\": " << options.mixed_login_ratio << ",\n";
    output << "  \"mixed_load_ratio\": " << options.mixed_load_ratio << ",\n";
    output << "  \"mixed_full_ratio\": " << options.mixed_full_ratio << ",\n";
    output << "  \"total_requests\": " << stats.total_requests << ",\n";
    output << "  \"successful_requests\": " << stats.successful_requests << ",\n";
    output << "  \"failed_requests\": " << stats.failed_requests << ",\n";
    output << "  \"scenario_successes\": " << stats.scenario_successes << ",\n";
    output << "  \"scenario_failures\": " << stats.scenario_failures << ",\n";
    output << "  \"qps\": " << std::fixed << std::setprecision(2) << stats.qps << ",\n";
    output << "  \"average_latency_ms\": " << std::fixed << std::setprecision(2) << stats.average_latency_ms << ",\n";
    output << "  \"p50_latency_ms\": " << stats.p50_latency_ms << ",\n";
    output << "  \"p95_latency_ms\": " << stats.p95_latency_ms << ",\n";
    output << "  \"p99_latency_ms\": " << stats.p99_latency_ms << ",\n";

    output << "  \"error_counts\": {";
    bool first = true;
    for (const auto& [error_code, count] : stats.error_counts) {
        if (!first) {
            output << ", ";
        }
        output << "\n    \"" << EscapeJson(error_code) << "\": " << count;
        first = false;
    }
    if (!stats.error_counts.empty()) {
        output << '\n';
    }
    output << "  },\n";

    output << "  \"operation_counts\": {";
    first = true;
    for (const auto& [operation, count] : stats.operation_counts) {
        if (!first) {
            output << ", ";
        }
        output << "\n    \"" << EscapeJson(operation) << "\": " << count;
        first = false;
    }
    if (!stats.operation_counts.empty()) {
        output << '\n';
    }
    output << "  }\n";
    output << "}\n";
}

void PrintSummary(const LoadOptions& options, const AggregateStats& stats) {
    const double success_rate = stats.total_requests == 0
                                    ? 0.0
                                    : (static_cast<double>(stats.successful_requests) /
                                       static_cast<double>(stats.total_requests)) *
                                          100.0;
    std::cout << "scenario=" << ScenarioToString(options.scenario) << '\n';
    std::cout << "seed_profile=" << SeedProfileToString(options.seed_profile) << '\n';
    std::cout << "concurrency=" << options.concurrency << '\n';
    std::cout << "duration_seconds=" << options.duration_seconds << '\n';
    std::cout << "account_count=" << options.account_count << '\n';
    if (options.scenario == LoadScenario::kMixed) {
        std::cout << "mixed_ratios=login:" << options.mixed_login_ratio << ",load:" << options.mixed_load_ratio
                  << ",full:" << options.mixed_full_ratio << '\n';
    }
    std::cout << "total_requests=" << stats.total_requests << '\n';
    std::cout << "successful_requests=" << stats.successful_requests << '\n';
    std::cout << "failed_requests=" << stats.failed_requests << '\n';
    std::cout << "scenario_successes=" << stats.scenario_successes << '\n';
    std::cout << "scenario_failures=" << stats.scenario_failures << '\n';
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "success_rate=" << success_rate << "%\n";
    std::cout << "qps=" << stats.qps << '\n';
    std::cout << "latency_avg_ms=" << stats.average_latency_ms << '\n';
    std::cout << "latency_p50_ms=" << stats.p50_latency_ms << '\n';
    std::cout << "latency_p95_ms=" << stats.p95_latency_ms << '\n';
    std::cout << "latency_p99_ms=" << stats.p99_latency_ms << '\n';

    std::cout << "operation_counts=";
    bool first = true;
    for (const auto& [operation, count] : stats.operation_counts) {
        if (!first) {
            std::cout << ',';
        }
        std::cout << operation << ':' << count;
        first = false;
    }
    std::cout << '\n';

    std::cout << "error_counts=";
    if (stats.error_counts.empty()) {
        std::cout << "none\n";
    } else {
        first = true;
        for (const auto& [error_code, count] : stats.error_counts) {
            if (!first) {
                std::cout << ',';
            }
            std::cout << error_code << ':' << count;
            first = false;
        }
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    common::log::Logger::Instance().SetServiceName("load_client");

    const auto options = ParseOptions(argc, argv);
    if (options.concurrency <= 0 || options.duration_seconds <= 0 || options.ramp_up_seconds < 0 ||
        options.think_time_ms < 0 || options.mixed_login_ratio < 0 || options.mixed_load_ratio < 0 ||
        options.mixed_full_ratio < 0) {
        std::cerr << "invalid numeric options\n";
        return 1;
    }
    if (options.scenario == LoadScenario::kMixed &&
        (options.mixed_login_ratio + options.mixed_load_ratio + options.mixed_full_ratio) <= 0) {
        std::cerr << "mixed scenario requires at least one positive ratio\n";
        return 1;
    }

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

    if ((options.scenario == LoadScenario::kLoginOnly || options.scenario == LoadScenario::kFullLoop ||
         options.scenario == LoadScenario::kMixed) &&
        (options.config_profile == "demo" || options.config_profile == "local")) {
        std::cerr << "warning: demo/local gateway configs enforce strict login rate limits; login-heavy scenarios may "
                     "report RATE_LIMITED unless you relax config or increase think time\n";
    }

    auto effective_options = options;
    if (effective_options.account_count <= 0) {
        effective_options.account_count = effective_options.concurrency;
    }
    if (effective_options.account_count < effective_options.concurrency) {
        effective_options.account_count = effective_options.concurrency;
    }

    const auto base_demo_data = demo::support::ReadDemoDataConfig(auth_config, player_query_config, dungeon_config);
    std::vector<UserContext> users;
    users.reserve(static_cast<std::size_t>(effective_options.account_count));
    for (int index = 0; index < effective_options.account_count; ++index) {
        users.push_back(
            {BuildUserConfig(base_demo_data,
                             effective_options.account_prefix,
                             index,
                             effective_options.account_count,
                             effective_options.seed_profile),
             {}});
    }

    if (effective_options.reset_demo_state) {
        ResetContext provisioner(auth_config, player_query_config, dungeon_config);
        if (!provisioner.Initialize()) {
            return 1;
        }
        std::string error_message;
        for (const auto& user : users) {
            if (!provisioner.EnsureAndReset(user.demo_config, &error_message)) {
                std::cerr << "failed to prepare user " << user.demo_config.account_name << ": " << error_message << '\n';
                return 1;
            }
        }
    }

    const auto connection_profile = BuildConnectionProfile(gate_config, api_config);
    if (effective_options.scenario == LoadScenario::kLoadOnly) {
        for (auto& user : users) {
            framework::transport::TransportClient gate_client(connection_profile.gate.host,
                                                              connection_profile.gate.port,
                                                              connection_profile.gate.timeout_ms,
                                                              connection_profile.gate.tls);
            StatsCollector ignored_stats;
            std::uint64_t request_id = 1;
            if (!RefreshSession(
                    connection_profile, gate_client, user, effective_options, &request_id, &ignored_stats, &user.session_id)) {
                std::cerr << "failed to pre-login user " << user.demo_config.account_name << '\n';
                return 1;
            }
        }
    }

    StatsCollector stats;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(effective_options.concurrency));
    const auto started_at = std::chrono::steady_clock::now();
    const auto deadline = started_at + std::chrono::seconds(effective_options.duration_seconds);

    for (int worker_index = 0; worker_index < effective_options.concurrency; ++worker_index) {
        workers.emplace_back([&, worker_index] {
            const auto initial_delay_ms = effective_options.concurrency <= 1
                                              ? 0
                                              : (effective_options.ramp_up_seconds * 1000 * worker_index) /
                                                    effective_options.concurrency;
            if (initial_delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(initial_delay_ms));
            }

            auto user = users[static_cast<std::size_t>(worker_index % effective_options.account_count)];
            framework::transport::TransportClient gate_client(connection_profile.gate.host,
                                                              connection_profile.gate.port,
                                                              connection_profile.gate.timeout_ms,
                                                              connection_profile.gate.tls);
            std::optional<ResetContext> resetter;
            if ((effective_options.scenario == LoadScenario::kFullLoop ||
                 effective_options.scenario == LoadScenario::kMixed) &&
                effective_options.reset_demo_state) {
                resetter.emplace(auth_config, player_query_config, dungeon_config);
                if (!resetter->Initialize()) {
                    stats.RecordScenario(false);
                    return;
                }
            }

            std::uint64_t request_id = static_cast<std::uint64_t>((worker_index + 1) * 1000000);
            std::uint32_t random_state = static_cast<std::uint32_t>(MixBits(static_cast<std::uint64_t>(user.demo_config.player_id)));
            while (std::chrono::steady_clock::now() < deadline) {
                const auto selected_scenario = effective_options.scenario == LoadScenario::kMixed
                                                   ? SelectMixedScenario(effective_options, &random_state)
                                                   : effective_options.scenario;
                const auto success = RunIterationForScenario(
                    selected_scenario,
                    connection_profile,
                    gate_client,
                    user,
                    effective_options,
                    &request_id,
                    &stats,
                    &user.session_id);
                stats.RecordScenario(success);

                if (selected_scenario == LoadScenario::kFullLoop && effective_options.reset_demo_state &&
                    resetter.has_value()) {
                    std::string error_message;
                    if (!resetter->ResetOnly(user.demo_config, &error_message)) {
                        std::cerr << "failed to reset user " << user.demo_config.account_name << ": " << error_message
                                  << '\n';
                    }
                    user.session_id.clear();
                    gate_client.Close();
                }

                if (effective_options.think_time_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(effective_options.think_time_ms));
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const auto aggregate = stats.Build(std::chrono::steady_clock::now() - started_at);
    PrintSummary(effective_options, aggregate);
    if (!effective_options.report_json_path.empty()) {
        WriteReportJson(effective_options.report_json_path, effective_options, aggregate);
    }

    return aggregate.total_requests == 0 ? 1 : 0;
}
