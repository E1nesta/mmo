// 代码规范落地：协议适配层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "internal/player_internal.grpc.pb.h"
#include "modules/player/application/player_query_service.h"
#include "modules/player/application/player_write_service.h"

namespace game_server::player {

class PlayerWriteServiceImpl final : public game_backend::internal::player::PlayerInternal::Service {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerWriteServiceImpl(PlayerQueryService& player_query_service, PlayerWriteService& player_write_service);

    ::grpc::Status GetPlayerSnapshot(::grpc::ServerContext* context,
                                     const game_backend::internal::player::GetPlayerSnapshotRequest* request,
                                     game_backend::internal::player::GetPlayerSnapshotResponse* response) override;

    ::grpc::Status InvalidatePlayerCache(::grpc::ServerContext* context,
                                         const game_backend::internal::player::InvalidatePlayerCacheRequest* request,
                                         game_backend::internal::player::InvalidatePlayerCacheResponse* response) override;
    ::grpc::Status GetBattleEntrySnapshot(::grpc::ServerContext* context,
                                          const game_backend::internal::player::GetBattleEntrySnapshotRequest* request,
                                          game_backend::internal::player::GetBattleEntrySnapshotResponse* response) override;
    ::grpc::Status PrepareBattleEntry(::grpc::ServerContext* context,
                                      const game_backend::internal::player::PrepareBattleEntryRequest* request,
                                      game_backend::internal::player::PrepareBattleEntryResponse* response) override;
    ::grpc::Status CancelBattleEntry(::grpc::ServerContext* context,
                                     const game_backend::internal::player::CancelBattleEntryRequest* request,
                                     game_backend::internal::player::CancelBattleEntryResponse* response) override;
    ::grpc::Status ApplyRewardGrant(::grpc::ServerContext* context,
                                    const game_backend::internal::player::ApplyRewardGrantRequest* request,
                                    game_backend::internal::player::ApplyRewardGrantResponse* response) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    PlayerQueryService& player_query_service_;
    PlayerWriteService& player_write_service_;
};

}  // namespace game_server::player
