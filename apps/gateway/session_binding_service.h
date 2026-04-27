// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/request_context.h"
#include "runtime/session/session_store.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace services::gateway {

class SessionBindingService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    struct ClientBinding {
        std::string auth_token;
        std::int64_t player_id = 0;
        std::int64_t account_id = 0;
        int line_no = 0;
        int anti_addi = 0;
        std::string device_id;
        std::string client_session_id;
        std::int64_t last_seen_epoch_ms = 0;
    };

    enum class Status {
        kBound,
        kRestored,
        kInvalid,
    };

    struct Result {
        Status status = Status::kInvalid;
        std::string reason;
    };

    explicit SessionBindingService(common::session::SessionReader& session_reader);

    [[nodiscard]] Result ValidateOrRestore(std::uint64_t connection_id,
                                           common::net::RequestContext* context);
    [[nodiscard]] Result ValidateBoundSession(std::uint64_t connection_id) const;
    void Bind(std::uint64_t connection_id,
              const std::string& auth_token,
              std::int64_t player_id,
              std::int64_t account_id);
    void UpdateClientMetadata(std::uint64_t connection_id,
                              int line_no,
                              int anti_addi,
                              const std::string& device_id,
                              const std::string& client_session_id);
    [[nodiscard]] bool Touch(std::uint64_t connection_id);
    [[nodiscard]] std::optional<std::uint64_t> FindConnectionIdByPlayerId(std::int64_t player_id) const;
    [[nodiscard]] std::optional<ClientBinding> FindByConnectionId(std::uint64_t connection_id) const;
    [[nodiscard]] std::optional<ClientBinding> FindByPlayerId(std::int64_t player_id) const;
    void Unbind(std::uint64_t connection_id);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<ClientBinding> RestoreBindingFromSession(
        common::net::RequestContext* context) const;
    [[nodiscard]] std::optional<common::model::Session> LoadActiveSession(const std::string& auth_token) const;
    void UpsertBindingLocked(std::uint64_t connection_id, ClientBinding binding);
    [[nodiscard]] static std::int64_t CurrentEpochMs();

    common::session::SessionReader& session_reader_;
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, ClientBinding> client_bindings_;
    std::unordered_map<std::int64_t, std::uint64_t> player_connections_;
};

}  // namespace services::gateway
