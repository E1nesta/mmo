// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/dungeon_runtime/dungeon_runtime_server_app.h"

#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/grpc/channel_factory.h"
#include "runtime/protocol/adapter_utils.h"
#include "runtime/protocol/proto_mapper.h"

#include "game_backend.pb.h"

#include <chrono>
#include <functional>

namespace services::dungeon_runtime {

namespace {

bool RequestPlayerMatchesContext(const framework::protocol::HandlerContext& context, std::int64_t request_player_id) {
    return request_player_id == 0 || request_player_id == context.request.player_id;
}

bool CheckRateLimit(common::redis::RedisClientPool& redis_pool,
                    const std::string& key,
                    int ttl_seconds,
                    std::int64_t limit) {
    auto redis = redis_pool.Acquire();
    std::string error_message;
    std::int64_t value = 0;
    if (!redis->IncrementWithExpire(key, ttl_seconds, &value, &error_message)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon rate limiter storage unavailable: " + error_message);
        return true;
    }
    return value <= limit;
}

bool UseInternalGrpcMtls(const common::config::SimpleConfig& config) {
    const auto environment = config.GetString("runtime.environment", "local");
    return environment == "staging" || environment == "prod";
}

framework::grpc::TlsChannelOptions BuildInternalGrpcTlsOptions() {
    framework::grpc::TlsChannelOptions options;
    options.root_cert_file = "/etc/game_backend/tls/ca.pem";
    options.cert_chain_file = "/etc/game_backend/tls/dungeon_runtime_server.crt";
    options.private_key_file = "/etc/game_backend/tls/dungeon_runtime_server.key";
    options.server_name = "player_write_grpc_server";
    return options;
}

std::uint16_t BuildIdGeneratorNodeId(const common::config::SimpleConfig& config) {
    const auto instance_id =
        config.GetString("service.instance_id", config.GetString("service.name", "dungeon_runtime_server"));
    const auto hashed = std::hash<std::string>{}(instance_id);
    return static_cast<std::uint16_t>((hashed % 1023U) + 1U);
}

std::string BuildSettleTokenSecret(const common::config::SimpleConfig& config) {
    return config.GetString(
        "security.settle_token.secret",
        config.GetString("security.trusted_gateway.shared_secret", "dungeon-settle-token"));
}

void FillProtoReward(const game_server::dungeon_runtime::Reward& reward, game_backend::proto::Reward* output) {
    if (output == nullptr) {
        return;
    }
    output->set_reward_type(reward.reward_type);
    output->set_amount(reward.amount);
}

std::vector<game_server::dungeon_runtime::DungeonUseItem> BuildUseItemList(
    const google::protobuf::RepeatedPtrField<game_backend::proto::DungeonUseItem>& input) {
    std::vector<game_server::dungeon_runtime::DungeonUseItem> output;
    output.reserve(static_cast<std::size_t>(input.size()));
    for (const auto& item : input) {
        output.push_back({item.item_id(), item.amount()});
    }
    return output;
}

game_server::dungeon_runtime::DungeonBattleStats BuildBattleStats(const game_backend::proto::DungeonBattleStats& input) {
    return {input.pass_time_seconds(),
            input.dungeon_rank(),
            input.master_be_hit(),
            input.use_ougi(),
            input.dodge_seconds(),
            input.heal_sum(),
            input.be_harm(),
            input.kill_enemy(),
            input.master_combo(),
            input.dungeon_report_json()};
}

common::net::Packet BuildEnterDungeonResponsePacket(const framework::protocol::HandlerContext& context,
                                                    const game_server::dungeon_runtime::EnterDungeonResponse& result) {
    game_backend::proto::EnterDungeonResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_session_id(result.session_id);
    response.set_remain_stamina(result.remain_stamina);
    response.set_seed(result.seed);
    response.set_settle_token(result.settle_token);
    return common::net::BuildPacket(
        common::net::MessageId::kEnterDungeonResponse, context.request.request_id, response);
}

common::net::Packet BuildSettleDungeonResponsePacket(const framework::protocol::HandlerContext& context,
                                                     const game_server::dungeon_runtime::SettleDungeonResponse& result) {
    game_backend::proto::SettleDungeonResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_reward_grant_id(result.reward_grant_id);
    response.set_grant_status(result.grant_status);
    for (const auto& reward : result.reward_preview) {
        FillProtoReward(reward, response.add_reward_preview());
    }
    for (const auto& item : result.use_item_list) {
        auto* output = response.add_use_item_list();
        output->set_item_id(item.item_id);
        output->set_amount(item.amount);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kSettleDungeonResponse, context.request.request_id, response);
}

common::net::Packet BuildGetSettlementGrantStatusResponsePacket(
    const framework::protocol::HandlerContext& context,
    const game_server::dungeon_runtime::SettlementGrantStatusResponse& result) {
    game_backend::proto::GetSettlementGrantStatusResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_reward_grant_id(result.reward_grant_id);
    response.set_grant_status(result.grant_status);
    for (const auto& reward : result.rewards) {
        FillProtoReward(reward, response.add_granted_rewards());
    }
    return common::net::BuildPacket(
        common::net::MessageId::kGetSettlementGrantStatusResponse, context.request.request_id, response);
}

common::net::Packet BuildGetActiveDungeonResponsePacket(
    const framework::protocol::HandlerContext& context,
    const game_server::dungeon_runtime::ActiveDungeonResponse& result) {
    game_backend::proto::GetActiveDungeonResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_found(result.found);
    if (result.found) {
        response.set_session_id(result.session_id);
        response.set_stage_id(result.stage_id);
        response.set_mode(result.mode);
        response.set_remain_stamina(result.remain_stamina);
        response.set_seed(result.seed);
        response.set_settle_token(result.settle_token);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kGetActiveDungeonResponse, context.request.request_id, response);
}

}  // namespace

DungeonRuntimeServerApp::DungeonRuntimeServerApp()
    : framework::service::ServiceApp("dungeon_runtime_server", "configs/dungeon_runtime_server.conf") {}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool DungeonRuntimeServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidateDungeonRuntimeStorageConfig(Config(), error_message)) {
        return false;
    }
    const auto mysql_options = common::mysql::ReadReadWritePoolOptions(Config(), "storage.battle.mysql.", 4);
    battle_writer_mysql_pool_ = std::make_unique<common::mysql::MySqlClientPool>(
        mysql_options.writer.connection, mysql_options.writer.pool_size);
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        Config(), "storage.runtime.redis.", "storage.battle.redis.", 4);
    runtime_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!battle_writer_mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!runtime_redis_pool_->Initialize(error_message)) {
        return false;
    }

    std::shared_ptr<::grpc::Channel> player_internal_channel;
    if (UseInternalGrpcMtls(Config())) {
        // 状态推进：`CreateTlsChannel` 执行写链或补偿并收敛状态变化。
        player_internal_channel = framework::grpc::CreateTlsChannel(
            framework::grpc::BuildAddress(Config(), "grpc.client.player_internal."),
            BuildInternalGrpcTlsOptions(),
            error_message);
        if (player_internal_channel == nullptr) {
            return false;
        }
    } else {
        player_internal_channel = framework::grpc::CreateInsecureChannel(Config(), "grpc.client.player_internal.");
    }

    player_snapshot_port_ =
        std::make_unique<game_server::dungeon_runtime::GrpcPlayerSnapshotPort>(std::move(player_internal_channel));
    stage_config_repository_ = std::make_unique<game_server::dungeon_runtime::InMemoryStageConfigRepository>(
        game_server::dungeon_runtime::InMemoryStageConfigRepository::FromConfig(Config()));
    dungeon_repository_ = std::make_unique<game_server::dungeon_runtime::MySqlDungeonRepository>(*battle_writer_mysql_pool_);
    dungeon_context_repository_ = std::make_unique<game_server::dungeon_runtime::RedisDungeonContextRepository>(
        *runtime_redis_pool_, Config().GetInt("storage.battle.context_ttl_seconds", 3600));
    player_lock_repository_ = std::make_unique<game_server::dungeon_runtime::RedisPlayerLockRepository>(
        *runtime_redis_pool_, Config().GetInt("storage.player.lock_ttl_seconds", 10));
    dungeon_runtime_service_ = std::make_unique<game_server::dungeon_runtime::DungeonRuntimeService>(
        *player_lock_repository_,
        *player_snapshot_port_,
        *stage_config_repository_,
        *dungeon_repository_,
        *dungeon_context_repository_,
        BuildIdGeneratorNodeId(Config()),
        BuildSettleTokenSecret(Config()));
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void DungeonRuntimeServerApp::RegisterRoutes() {
    Routes().Register(common::net::MessageId::kEnterDungeonRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleEnterDungeonRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kSettleDungeonRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleSettleDungeonRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kGetActiveDungeonRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleGetActiveDungeonRequest(context, packet);
                      });
    Routes().Register(
        common::net::MessageId::kGetSettlementGrantStatusRequest,
        [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
            return HandleGetSettlementGrantStatusRequest(context, packet);
        });
}

// 请求处理：`HandleEnterDungeonRequest` 承接边界输入并转发到目标链路。
common::net::Packet DungeonRuntimeServerApp::HandleEnterDungeonRequest(const framework::protocol::HandlerContext& context,
                                                                       const common::net::Packet& packet) const {
    const auto started_at = std::chrono::steady_clock::now();
    game_backend::proto::EnterDungeonRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid enter dungeon request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    if (!CheckRateLimit(*runtime_redis_pool_,
                        "rate:dungeon:enter:" + std::to_string(context.request.player_id),
                        Config().GetInt("security.rate_limit.dungeon_enter.ttl_seconds", 5),
                        Config().GetInt("security.rate_limit.dungeon_enter.limit", 8))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter rate limited: player_id=" + std::to_string(context.request.player_id) + " stage_id=" +
                std::to_string(request.stage_id()));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "dungeon enter rate limited");
    }

    const auto result = dungeon_runtime_service_->EnterDungeon(
        {context.request.player_id,
         context.request.request_id,
         request.stage_id(),
         request.mode().empty() ? "pve" : request.mode(),
        request.loadout_id(),
         BuildUseItemList(request.use_item_list())},
        context.request.trace_id);
    if (!result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter failed: player_id=" + std::to_string(context.request.player_id) + " stage_id=" +
                std::to_string(request.stage_id()) + " error=" + std::string(common::error::ToString(result.error_code)));
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "dungeon enter success: player_id=" + std::to_string(context.request.player_id) + " stage_id=" +
            std::to_string(request.stage_id()) + " session_id=" + std::to_string(result.session_id) +
            " remain_stamina=" + std::to_string(result.remain_stamina) + " latency_ms=" +
            std::to_string(latency_ms.count()));
    return BuildEnterDungeonResponsePacket(context, result);
}

// 请求处理：`HandleSettleDungeonRequest` 承接边界输入并转发到目标链路。
common::net::Packet DungeonRuntimeServerApp::HandleSettleDungeonRequest(const framework::protocol::HandlerContext& context,
                                                                        const common::net::Packet& packet) const {
    const auto started_at = std::chrono::steady_clock::now();
    game_backend::proto::SettleDungeonRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid settle dungeon request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    if (!CheckRateLimit(*runtime_redis_pool_,
                        "rate:dungeon:settle:" + std::to_string(context.request.player_id) + ":" +
                            std::to_string(request.session_id()),
                        Config().GetInt("security.rate_limit.dungeon_settle.ttl_seconds", 5),
                        Config().GetInt("security.rate_limit.dungeon_settle.limit", 6))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle rate limited: player_id=" + std::to_string(context.request.player_id) + " session_id=" +
                std::to_string(request.session_id()));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "dungeon settle rate limited");
    }

    const auto result = dungeon_runtime_service_->SettleDungeon({context.request.player_id,
                                                                 request.session_id(),
                                                                 request.stage_id(),
                                                                 request.star(),
                                                                 request.result_code(),
                                                                 request.client_score(),
                                                                 request.settle_token(),
                                                                 request.has_battle_stats()
                                                                     ? BuildBattleStats(request.battle_stats())
                                                                     : game_server::dungeon_runtime::DungeonBattleStats{}},
                                                                context.request.trace_id);
    if (!result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle failed: player_id=" + std::to_string(context.request.player_id) + " session_id=" +
                std::to_string(request.session_id()) + " stage_id=" + std::to_string(request.stage_id()) +
                " error=" + std::string(common::error::ToString(result.error_code)));
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "dungeon settle success: player_id=" + std::to_string(context.request.player_id) + " session_id=" +
            std::to_string(request.session_id()) + " stage_id=" + std::to_string(request.stage_id()) +
            " grant_id=" + std::to_string(result.reward_grant_id) + " grant_status=" +
            std::to_string(result.grant_status) + " latency_ms=" + std::to_string(latency_ms.count()));
    return BuildSettleDungeonResponsePacket(context, result);
}

// 请求处理：`HandleGetActiveDungeonRequest` 承接边界输入并转发到目标链路。
common::net::Packet DungeonRuntimeServerApp::HandleGetActiveDungeonRequest(
    const framework::protocol::HandlerContext& context,
    const common::net::Packet& packet) const {
    game_backend::proto::GetActiveDungeonRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid get active dungeon request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    const auto result = dungeon_runtime_service_->GetActiveDungeon(context.request.player_id);
    if (!result.success) {
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    return BuildGetActiveDungeonResponsePacket(context, result);
}

// 请求处理：`HandleGetSettlementGrantStatusRequest` 承接边界输入并转发到目标链路。
common::net::Packet DungeonRuntimeServerApp::HandleGetSettlementGrantStatusRequest(
    const framework::protocol::HandlerContext& context,
    const common::net::Packet& packet) const {
    game_backend::proto::GetSettlementGrantStatusRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid get settlement grant status request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    const auto result = dungeon_runtime_service_->GetSettlementGrantStatus(
        context.request.player_id, request.reward_grant_id());
    if (!result.success) {
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    return BuildGetSettlementGrantStatusResponsePacket(context, result);
}

}  // namespace services::dungeon_runtime
