// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/player_query/player_query_server_app.h"

#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/protocol/adapter_utils.h"
#include "runtime/protocol/proto_mapper.h"

#include "game_backend.pb.h"

#include <chrono>

namespace services::player_query {

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
            "player query rate limiter storage unavailable: " + error_message);
        return true;
    }
    return value <= limit;
}

void FillProtoPlayerDisplay(const common::model::HomePlayerDisplay& display,
                            game_backend::proto::HomePlayerDisplay* output) {
    if (output == nullptr) {
        return;
    }

    output->set_player_id(display.player_id);
    output->set_account_id(display.account_id);
    output->set_server_id(display.server_id);
    output->set_player_name(display.player_name);
    output->set_nickname(display.nickname);
    output->set_level(display.level);
}

void FillProtoResourceSummary(const common::model::HomeResourceSummary& summary,
                              game_backend::proto::HomeResourceSummary* output) {
    if (output == nullptr) {
        return;
    }

    output->set_stamina(summary.stamina);
    output->set_gold(summary.gold);
    output->set_diamond(summary.diamond);
    output->clear_currencies();
    for (const auto& currency : summary.currencies) {
        auto* item = output->add_currencies();
        item->set_currency_type(currency.currency_type);
        item->set_amount(currency.amount);
    }
}

void FillProtoCharacterSummary(const common::model::HomeCharacterSummary& summary,
                               game_backend::proto::HomeCharacterSummary* output) {
    if (output == nullptr) {
        return;
    }

    output->set_fight_power(summary.fight_power);
    output->clear_role_summaries();
    for (const auto& role : summary.role_summaries) {
        auto* item = output->add_role_summaries();
        item->set_role_id(role.role_id);
        item->set_level(role.level);
        item->set_star(role.star);
    }
}

void FillProtoDungeonSummary(const common::model::HomeDungeonSummary& summary,
                             game_backend::proto::HomeDungeonSummary* output) {
    if (output == nullptr) {
        return;
    }

    output->set_main_stage_id(summary.main_stage_id);
    output->set_main_chapter_id(summary.main_chapter_id);
    output->set_stage_progress_count(summary.stage_progress_count);
    output->clear_stage_progress();
    for (const auto& progress : summary.stage_progress) {
        auto* item = output->add_stage_progress();
        item->set_stage_id(progress.stage_id);
        item->set_best_star(progress.best_star);
        item->set_is_first_clear(progress.is_first_clear);
    }
}

void FillProtoHomeInit(const common::model::HomeInitView& view, game_backend::proto::HomeInitData* output) {
    if (output == nullptr) {
        return;
    }

    FillProtoPlayerDisplay(view.player_display, output->mutable_player_display());
    FillProtoResourceSummary(view.resource_summary, output->mutable_resource_summary());
    FillProtoCharacterSummary(view.character_summary, output->mutable_character_summary());
    FillProtoDungeonSummary(view.dungeon_summary, output->mutable_dungeon_summary());
    output->mutable_activity_summary()->set_hydrated(view.activity_summary.hydrated);
    output->mutable_entry_state_summary()->set_hydrated(view.entry_state_summary.hydrated);
}

common::net::Packet BuildPlayerInitResponsePacket(const framework::protocol::HandlerContext& context,
                                                  const game_server::player::LoadPlayerResponse& result) {
    game_backend::proto::LoadPlayerResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_loaded_from_cache(result.loaded_from_cache);
    FillProtoHomeInit(result.home_init, response.mutable_home_init());
    return common::net::BuildPacket(common::net::MessageId::kPlayerInitResponse, context.request.request_id, response);
}

common::net::Packet BuildPlayerSnapshotResponsePacket(const framework::protocol::HandlerContext& context,
                                                      const game_server::player::PlayerSnapshotResponse& result) {
    game_backend::proto::PlayerSnapshotResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_found(result.found);
    response.set_player_id(result.player_id);
    response.set_level(result.level);
    response.set_stamina(result.stamina);
    response.set_nickname(result.nickname);
    response.set_gold(result.gold);
    response.set_diamond(result.diamond);
    response.set_main_stage_id(result.main_stage_id);
    response.set_main_chapter_id(result.main_chapter_id);
    response.set_fight_power(result.fight_power);
    response.set_stage_progress_count(result.stage_progress_count);
    for (const auto& currency : result.currencies) {
        auto* item = response.add_currencies();
        item->set_currency_type(currency.currency_type);
        item->set_amount(currency.amount);
    }
    for (const auto& role : result.role_summaries) {
        auto* item = response.add_role_summaries();
        item->set_role_id(role.role_id);
        item->set_level(role.level);
        item->set_star(role.star);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kPlayerSnapshotResponse, context.request.request_id, response);
}

}  // namespace

PlayerQueryServerApp::PlayerQueryServerApp()
    : framework::service::ServiceApp("player_query_server", "configs/player_query_server.conf") {}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool PlayerQueryServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidatePlayerQueryStorageConfig(Config(), error_message)) {
        return false;
    }
    mysql_pool_ = std::make_unique<common::mysql::MySqlReadWriteClientPool>(
        common::mysql::ReadReadWritePoolOptions(Config(), "storage.player.mysql.", 4));
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        Config(), "storage.player_cache.redis.", "storage.player.redis.", 4);
    player_cache_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!player_cache_redis_pool_->Initialize(error_message)) {
        return false;
    }

    player_repository_ = std::make_unique<game_server::player::ReadWriteMySqlPlayerRepository>(*mysql_pool_);
    player_cache_repository_ = std::make_unique<game_server::player::RedisPlayerCacheRepository>(
        *player_cache_redis_pool_, Config().GetInt("storage.player.snapshot_ttl_seconds", 300));
    player_query_service_ =
        std::make_unique<game_server::player::PlayerQueryService>(*player_repository_, *player_cache_repository_);
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void PlayerQueryServerApp::RegisterRoutes() {
    Routes().Register(common::net::MessageId::kPlayerInitRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandlePlayerInitRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kPlayerSnapshotRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandlePlayerSnapshotRequest(context, packet);
                      });
}

// 请求处理：`HandlePlayerInitRequest` 承接边界输入并转发到目标链路。
common::net::Packet PlayerQueryServerApp::HandlePlayerInitRequest(const framework::protocol::HandlerContext& context,
                                                                  const common::net::Packet& packet) const {
    const auto started_at = std::chrono::steady_clock::now();
    game_backend::proto::LoadPlayerRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid player init request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    if (!CheckRateLimit(*player_cache_redis_pool_,
                        "rate:player_query:init:" + std::to_string(context.request.player_id),
                        Config().GetInt("security.rate_limit.player_init.ttl_seconds", 5),
                        Config().GetInt("security.rate_limit.player_init.limit", 10))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "player init rate limited: player_id=" + std::to_string(context.request.player_id));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "player init rate limited");
    }

    const auto result = player_query_service_->LoadPlayer(context.request.player_id);
    if (!result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "player init failed: player_id=" + std::to_string(context.request.player_id) + " error=" +
                std::string(common::error::ToString(result.error_code)));
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "player init success: player_id=" + std::to_string(context.request.player_id) + " loaded_from_cache=" +
            std::string(result.loaded_from_cache ? "true" : "false") + " latency_ms=" +
            std::to_string(latency_ms.count()) + " cache_hit_total=" +
            std::to_string(player_query_service_->CacheHitCount()) + " cache_miss_total=" +
            std::to_string(player_query_service_->CacheMissCount()) + " storage_load_total=" +
            std::to_string(player_query_service_->StorageLoadCount()) + " reader_hit_total=" +
            std::to_string(player_repository_->ReaderHitCount()) + " writer_fallback_total=" +
            std::to_string(player_repository_->WriterFallbackCount()));
    return BuildPlayerInitResponsePacket(context, result);
}

// 请求处理：`HandlePlayerSnapshotRequest` 承接边界输入并转发到目标链路。
common::net::Packet PlayerQueryServerApp::HandlePlayerSnapshotRequest(const framework::protocol::HandlerContext& context,
                                                                      const common::net::Packet& packet) const {
    const auto started_at = std::chrono::steady_clock::now();
    game_backend::proto::PlayerSnapshotRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid player snapshot request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request,
            common::error::ErrorCode::kRequestContextInvalid,
            "request player_id does not match authenticated session");
    }

    if (!CheckRateLimit(*player_cache_redis_pool_,
                        "rate:player_query:snapshot:" + std::to_string(context.request.player_id),
                        Config().GetInt("security.rate_limit.player_snapshot.ttl_seconds", 5),
                        Config().GetInt("security.rate_limit.player_snapshot.limit", 20))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "player snapshot rate limited: player_id=" + std::to_string(context.request.player_id));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "player snapshot rate limited");
    }

    const auto result = player_query_service_->GetPlayerSnapshot(context.request.player_id);
    if (!result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "player snapshot failed: player_id=" + std::to_string(context.request.player_id) + " error=" +
                std::string(common::error::ToString(result.error_code)));
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "player snapshot success: player_id=" + std::to_string(context.request.player_id) + " found=" +
            std::string(result.found ? "true" : "false") + " latency_ms=" +
            std::to_string(latency_ms.count()) + " cache_hit_total=" +
            std::to_string(player_query_service_->CacheHitCount()) + " cache_miss_total=" +
            std::to_string(player_query_service_->CacheMissCount()) + " storage_load_total=" +
            std::to_string(player_query_service_->StorageLoadCount()) + " reader_hit_total=" +
            std::to_string(player_repository_->ReaderHitCount()) + " writer_fallback_total=" +
            std::to_string(player_repository_->WriterFallbackCount()));
    return BuildPlayerSnapshotResponsePacket(context, result);
}

}  // namespace services::player_query
