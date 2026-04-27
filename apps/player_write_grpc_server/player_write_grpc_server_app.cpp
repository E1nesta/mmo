// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/player_write_grpc_server/player_write_grpc_server_app.h"

#include "runtime/foundation/build/build_info.h"
#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/grpc/server_runner.h"
#include "runtime/transport/service_options.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace services::player_write {

namespace {

std::atomic_bool g_running{true};

bool UseInternalGrpcMtls(const common::config::SimpleConfig& config) {
    const auto environment = config.GetString("runtime.environment", "local");
    return environment == "staging" || environment == "prod";
}

framework::grpc::TlsServerCredentialsOptions BuildInternalGrpcTlsOptions() {
    framework::grpc::TlsServerCredentialsOptions options;
    options.cert_chain_file = "/etc/game_backend/tls/player_write_grpc_server.crt";
    options.private_key_file = "/etc/game_backend/tls/player_write_grpc_server.key";
    options.client_ca_file = "/etc/game_backend/tls/ca.pem";
    options.require_client_auth = true;
    return options;
}

void HandleSignal(int /*signal*/) {
    g_running.store(false);
}

}  // namespace

PlayerWriteGrpcServerApp::PlayerWriteGrpcServerApp(std::string default_service_name, std::string default_config_path)
    : default_service_name_(std::move(default_service_name)),
      default_config_path_(std::move(default_config_path)) {}

// 服务主流程：`Main` 串联启动、运行与收敛阶段。
int PlayerWriteGrpcServerApp::Main(int argc, char* argv[]) {
    g_running.store(true);
    auto options = framework::runtime::ParseServiceOptions(argc, argv, default_service_name_, default_config_path_);
    if (options.show_version) {
        std::cout << common::build::Version() << '\n';
        return 0;
    }

    auto& logger = common::log::Logger::Instance();
    logger.SetServiceName(options.service_name);
    if (!config_.LoadFromFile(options.config_path)) {
        logger.LogSync(common::log::LogLevel::kError, "failed to load grpc server config");
        return 1;
    }

    logger.SetServiceName(config_.GetString("service.name", options.service_name));
    logger.SetServiceInstanceId(
        config_.GetString("service.instance_id", config_.GetString("service.name", options.service_name)));
    logger.SetEnvironment(config_.GetString("runtime.environment", "local"));
    logger.SetMinLogLevel(config_.GetString("log.level", "info"));
    logger.SetLogFormat(config_.GetString("log.format", "auto"));

    std::string error_message;
    if (!BuildDependencies(&error_message)) {
        logger.LogSync(common::log::LogLevel::kError, error_message);
        return 1;
    }
    if (options.check_only) {
        logger.LogSync(common::log::LogLevel::kInfo, "grpc server configuration and dependencies check passed");
        Shutdown();
        return 0;
    }
    if (!StartServer(&error_message)) {
        logger.LogSync(common::log::LogLevel::kError, error_message);
        Shutdown();
        return 1;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    auto* server_runner = server_runner_.get();
    std::thread shutdown_watcher([server_runner] {
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (server_runner != nullptr) {
            server_runner->Shutdown();
        }
    });

    const auto host = config_.GetString("grpc.listen.host", "0.0.0.0");
    const auto port = config_.GetInt("grpc.listen.port", 7400);
    logger.Log(
        common::log::LogLevel::kInfo,
        "player write grpc server listening on " + host + ":" + std::to_string(port));
    if (server_runner_ != nullptr) {
        server_runner_->Wait();
    }
    g_running.store(false);
    if (shutdown_watcher.joinable()) {
        shutdown_watcher.join();
    }

    Shutdown();
    logger.Flush();
    logger.Shutdown();
    return 0;
}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool PlayerWriteGrpcServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidatePlayerWriteStorageConfig(config_, error_message)) {
        return false;
    }
    const auto mysql_options = common::mysql::ReadReadWritePoolOptions(config_, "storage.player.mysql.", 4);
    player_writer_mysql_pool_ = std::make_unique<common::mysql::MySqlClientPool>(
        mysql_options.writer.connection, mysql_options.writer.pool_size);
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        config_, "storage.player_cache.redis.", "storage.player.redis.", 4);
    player_cache_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!player_writer_mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!player_cache_redis_pool_->Initialize(error_message)) {
        return false;
    }

    player_repository_ = std::make_unique<game_server::player::MySqlPlayerRepository>(*player_writer_mysql_pool_);
    player_cache_repository_ = std::make_unique<game_server::player::RedisPlayerCacheRepository>(
        *player_cache_redis_pool_, config_.GetInt("storage.player.snapshot_ttl_seconds", 300));
    player_query_service_ =
        std::make_unique<game_server::player::PlayerQueryService>(*player_repository_, *player_cache_repository_);
    player_write_service_ =
        std::make_unique<game_server::player::PlayerWriteService>(*player_repository_, *player_cache_repository_);
    grpc_service_ =
        std::make_unique<game_server::player::PlayerWriteServiceImpl>(*player_query_service_, *player_write_service_);
    return true;
}

bool PlayerWriteGrpcServerApp::StartServer(std::string* error_message) {
    server_runner_ = std::make_unique<framework::grpc::ServerRunner>(
        config_.GetString("grpc.listen.host", "0.0.0.0"),
        config_.GetInt("grpc.listen.port", 7400));
    const auto started = UseInternalGrpcMtls(config_)
                             ? server_runner_->Start(grpc_service_.get(), BuildInternalGrpcTlsOptions(), error_message)
                             : server_runner_->Start(grpc_service_.get(), error_message);
    if (!started) {
        server_runner_.reset();
        return false;
    }
    return true;
}

// 服务主流程：`Shutdown` 串联启动、运行与收敛阶段。
void PlayerWriteGrpcServerApp::Shutdown() {
    if (server_runner_ != nullptr) {
        server_runner_->Shutdown();
        server_runner_.reset();
    }
    grpc_service_.reset();
    player_write_service_.reset();
    player_query_service_.reset();
    player_cache_repository_.reset();
    player_repository_.reset();
    player_cache_redis_pool_.reset();
    player_writer_mysql_pool_.reset();
}

}  // namespace services::player_write
