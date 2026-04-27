// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/domain/reward.h"
#include "runtime/foundation/error/error_code.h"
#include "modules/dungeon_runtime/domain/player_snapshot.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game_server::dungeon_runtime {

struct GetDungeonEntrySnapshotPortResponse {
    bool success = false;
    bool found = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    PlayerSnapshot snapshot;
};

struct PrepareDungeonEntryPortResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    int remain_energy = 0;
};

struct CancelDungeonEntryPortResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
};

struct ApplyRewardGrantPortResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::vector<Reward> rewards;
};

class PlayerSnapshotPort {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~PlayerSnapshotPort() = default;

    [[nodiscard]] virtual GetDungeonEntrySnapshotPortResponse GetDungeonEntrySnapshot(std::int64_t player_id) const = 0;
    virtual bool InvalidatePlayerSnapshot(std::int64_t player_id) = 0;
    [[nodiscard]] virtual PrepareDungeonEntryPortResponse PrepareDungeonEntry(std::int64_t player_id,
                                                                            std::int64_t session_id,
                                                                            int energy_cost,
                                                                            const std::string& idempotency_key) = 0;
    [[nodiscard]] virtual CancelDungeonEntryPortResponse CancelDungeonEntry(std::int64_t player_id,
                                                                          std::int64_t session_id,
                                                                          int energy_refund,
                                                                          const std::string& idempotency_key) = 0;
    [[nodiscard]] virtual ApplyRewardGrantPortResponse ApplyRewardGrant(std::int64_t player_id,
                                                                        std::int64_t grant_id,
                                                                        std::int64_t session_id,
                                                                        const std::vector<Reward>& rewards,
                                                                        const std::string& idempotency_key) = 0;
};

}  // namespace game_server::dungeon_runtime
