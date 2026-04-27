// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/ports/player_snapshot_port.h"
#include "internal/player_internal.grpc.pb.h"

#include <grpcpp/channel.h>

#include <memory>

namespace game_server::dungeon_runtime {

class GrpcPlayerSnapshotPort final : public PlayerSnapshotPort {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit GrpcPlayerSnapshotPort(std::shared_ptr<::grpc::Channel> channel);

    [[nodiscard]] GetDungeonEntrySnapshotPortResponse GetDungeonEntrySnapshot(std::int64_t player_id) const override;
    bool InvalidatePlayerSnapshot(std::int64_t player_id) override;
    [[nodiscard]] PrepareDungeonEntryPortResponse PrepareDungeonEntry(std::int64_t player_id,
                                                                    std::int64_t session_id,
                                                                    int energy_cost,
                                                                    const std::string& idempotency_key) override;
    [[nodiscard]] CancelDungeonEntryPortResponse CancelDungeonEntry(std::int64_t player_id,
                                                                  std::int64_t session_id,
                                                                  int energy_refund,
                                                                  const std::string& idempotency_key) override;
    [[nodiscard]] ApplyRewardGrantPortResponse ApplyRewardGrant(std::int64_t player_id,
                                                                std::int64_t grant_id,
                                                                std::int64_t session_id,
                                                                const std::vector<Reward>& rewards,
                                                                const std::string& idempotency_key) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    std::unique_ptr<game_backend::internal::player::PlayerInternal::Stub> stub_;
};

}  // namespace game_server::dungeon_runtime
