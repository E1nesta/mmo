// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/gateway/session_binding_service.h"

#include <chrono>

namespace services::gateway {

SessionBindingService::SessionBindingService(common::session::SessionReader& session_reader)
    : session_reader_(session_reader) {}

// 输入校验：`ValidateOrRestore` 校验关键约束并在失败时快速返回。
SessionBindingService::Result SessionBindingService::ValidateOrRestore(
    std::uint64_t connection_id,
    common::net::RequestContext* context) {
    if (context == nullptr) {
        return {Status::kInvalid, "request context is null"};
    }

    {
        std::lock_guard lock(mutex_);
        const auto binding_iter = client_bindings_.find(connection_id);
        if (binding_iter != client_bindings_.end()) {
            if (!context->auth_token.empty() && binding_iter->second.auth_token != context->auth_token) {
                return {Status::kInvalid, "connection session binding mismatch"};
            }

            if (context->player_id != 0 && binding_iter->second.player_id != context->player_id) {
                return {Status::kInvalid, "connection session binding mismatch"};
            }

            if (context->account_id != 0 && binding_iter->second.account_id != context->account_id) {
                return {Status::kInvalid, "connection session binding mismatch"};
            }

            if (context->auth_token.empty()) {
                context->auth_token = binding_iter->second.auth_token;
            }
            if (auto session = LoadActiveSession(binding_iter->second.auth_token); !session.has_value()) {
                return {Status::kInvalid, "bound session is no longer active"};
            }
            context->player_id = binding_iter->second.player_id;
            context->account_id = binding_iter->second.account_id;
            return {Status::kBound, {}};
        }
    }

    const auto restored_binding = RestoreBindingFromSession(context);
    if (!restored_binding.has_value()) {
        return {Status::kInvalid, "session restore failed"};
    }

    std::lock_guard lock(mutex_);
    UpsertBindingLocked(connection_id, *restored_binding);
    return {Status::kRestored, {}};
}

// 输入校验：`ValidateBoundSession` 校验关键约束并在失败时快速返回。
SessionBindingService::Result SessionBindingService::ValidateBoundSession(std::uint64_t connection_id) const {
    std::lock_guard lock(mutex_);
    const auto iter = client_bindings_.find(connection_id);
    if (iter == client_bindings_.end()) {
        return {Status::kInvalid, "connection is not bound"};
    }
    if (!LoadActiveSession(iter->second.auth_token).has_value()) {
        return {Status::kInvalid, "bound session is no longer active"};
    }
    return {Status::kBound, {}};
}

void SessionBindingService::Bind(std::uint64_t connection_id,
                                 const std::string& auth_token,
                                 std::int64_t player_id,
                                 std::int64_t account_id) {
    std::lock_guard lock(mutex_);
    UpsertBindingLocked(connection_id, {auth_token, player_id, account_id, 0, 0, {}, {}, CurrentEpochMs()});
}

// 状态推进：`UpdateClientMetadata` 执行写链或补偿并收敛状态变化。
void SessionBindingService::UpdateClientMetadata(std::uint64_t connection_id,
                                                 int line_no,
                                                 int anti_addi,
                                                 const std::string& device_id,
                                                 const std::string& client_session_id) {
    std::lock_guard lock(mutex_);
    const auto iter = client_bindings_.find(connection_id);
    if (iter == client_bindings_.end()) {
        return;
    }
    if (line_no > 0) {
        iter->second.line_no = line_no;
    }
    iter->second.anti_addi = anti_addi;
    if (!device_id.empty()) {
        iter->second.device_id = device_id;
    }
    if (!client_session_id.empty()) {
        iter->second.client_session_id = client_session_id;
    }
    iter->second.last_seen_epoch_ms = CurrentEpochMs();
}

bool SessionBindingService::Touch(std::uint64_t connection_id) {
    std::lock_guard lock(mutex_);
    const auto iter = client_bindings_.find(connection_id);
    if (iter == client_bindings_.end()) {
        return false;
    }
    iter->second.last_seen_epoch_ms = CurrentEpochMs();
    return true;
}

// 状态读取：`FindConnectionIdByPlayerId` 负责加载上下文并返回稳定结果。
std::optional<std::uint64_t> SessionBindingService::FindConnectionIdByPlayerId(std::int64_t player_id) const {
    std::lock_guard lock(mutex_);
    if (const auto iter = player_connections_.find(player_id); iter != player_connections_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

// 状态读取：`FindByConnectionId` 负责加载上下文并返回稳定结果。
std::optional<SessionBindingService::ClientBinding> SessionBindingService::FindByConnectionId(
    std::uint64_t connection_id) const {
    std::lock_guard lock(mutex_);
    if (const auto iter = client_bindings_.find(connection_id); iter != client_bindings_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

// 状态读取：`FindByPlayerId` 负责加载上下文并返回稳定结果。
std::optional<SessionBindingService::ClientBinding> SessionBindingService::FindByPlayerId(
    std::int64_t player_id) const {
    std::lock_guard lock(mutex_);
    const auto player_iter = player_connections_.find(player_id);
    if (player_iter == player_connections_.end()) {
        return std::nullopt;
    }
    if (const auto binding_iter = client_bindings_.find(player_iter->second); binding_iter != client_bindings_.end()) {
        return binding_iter->second;
    }
    return std::nullopt;
}

void SessionBindingService::Unbind(std::uint64_t connection_id) {
    std::lock_guard lock(mutex_);
    if (const auto iter = client_bindings_.find(connection_id); iter != client_bindings_.end()) {
        const auto player_iter = player_connections_.find(iter->second.player_id);
        if (player_iter != player_connections_.end() && player_iter->second == connection_id) {
            player_connections_.erase(player_iter);
        }
        client_bindings_.erase(iter);
    }
}

std::optional<SessionBindingService::ClientBinding> SessionBindingService::RestoreBindingFromSession(
    common::net::RequestContext* context) const {
    if (context == nullptr || context->auth_token.empty()) {
        return std::nullopt;
    }

    const auto session = LoadActiveSession(context->auth_token);
    if (!session.has_value()) {
        return std::nullopt;
    }

    if (context->player_id != 0 && session->player_id != context->player_id) {
        return std::nullopt;
    }

    if (context->account_id != 0 && session->account_id != context->account_id) {
        return std::nullopt;
    }

    context->player_id = session->player_id;
    context->account_id = session->account_id;
    return ClientBinding{
        session->session_id, session->player_id, session->account_id, 0, 0, session->device_id, {}, CurrentEpochMs()};
}

// 状态读取：`LoadActiveSession` 负责加载上下文并返回稳定结果。
std::optional<common::model::Session> SessionBindingService::LoadActiveSession(const std::string& auth_token) const {
    const auto session = session_reader_.FindById(auth_token);
    if (!session.has_value()) {
        return std::nullopt;
    }
    if (session->status != common::model::SessionStatus::kActive) {
        return std::nullopt;
    }
    if (session->expires_at_epoch_seconds > 0 &&
        session->expires_at_epoch_seconds < static_cast<std::int64_t>(
                                               std::chrono::duration_cast<std::chrono::seconds>(
                                                   std::chrono::system_clock::now().time_since_epoch())
                                                   .count())) {
        return std::nullopt;
    }
    return session;
}

void SessionBindingService::UpsertBindingLocked(std::uint64_t connection_id, ClientBinding binding) {
    binding.last_seen_epoch_ms = CurrentEpochMs();
    if (binding.player_id != 0) {
        if (const auto existing = player_connections_.find(binding.player_id);
            existing != player_connections_.end() && existing->second != connection_id) {
            client_bindings_.erase(existing->second);
        }
        player_connections_[binding.player_id] = connection_id;
    }
    client_bindings_[connection_id] = std::move(binding);
}

std::int64_t SessionBindingService::CurrentEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace services::gateway
