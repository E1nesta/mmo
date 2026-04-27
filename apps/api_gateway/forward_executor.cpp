// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/api_gateway/forward_executor.h"

namespace services::api_gateway {

ApiGatewayForwardExecutor::ApiGatewayForwardExecutor(Options options)
    : auth_upstream_(std::make_unique<framework::transport::UpstreamClientPool>(
          options.auth.host, options.auth.port, options.auth.timeout_ms, options.auth.pool_size, options.auth.tls)),
      player_query_upstream_(std::make_unique<framework::transport::UpstreamClientPool>(
          options.player_query.host,
          options.player_query.port,
          options.player_query.timeout_ms,
          options.player_query.pool_size,
          options.player_query.tls)),
      dungeon_runtime_upstream_(std::make_unique<framework::transport::UpstreamClientPool>(
          options.dungeon_runtime.host,
          options.dungeon_runtime.port,
          options.dungeon_runtime.timeout_ms,
          options.dungeon_runtime.pool_size,
          options.dungeon_runtime.tls)),
      social_upstream_(std::make_unique<framework::transport::UpstreamClientPool>(
          options.social.host, options.social.port, options.social.timeout_ms, options.social.pool_size, options.social.tls)) {}

bool ApiGatewayForwardExecutor::SendAndReceive(common::net::MessageId message_id,
                                               const common::net::Packet& request_packet,
                                               common::net::Packet* response_packet,
                                               std::string* error_message,
                                               framework::transport::TransportFailureCode* failure_code) {
    return ResolveUpstream(message_id).SendAndReceive(request_packet, response_packet, error_message, failure_code);
}

// 状态读取：`ResolveUpstream` 负责加载上下文并返回稳定结果。
framework::transport::UpstreamClientPool& ApiGatewayForwardExecutor::ResolveUpstream(common::net::MessageId message_id) {
    switch (message_id) {
    case common::net::MessageId::kAuthLoginRequest:
        return *auth_upstream_;
    case common::net::MessageId::kPlayerInitRequest:
    case common::net::MessageId::kPlayerSnapshotRequest:
        return *player_query_upstream_;
    case common::net::MessageId::kEnterDungeonRequest:
    case common::net::MessageId::kSettleDungeonRequest:
    case common::net::MessageId::kGetActiveDungeonRequest:
    case common::net::MessageId::kGetSettlementGrantStatusRequest:
        return *dungeon_runtime_upstream_;
    case common::net::MessageId::kListFriendsRequest:
    case common::net::MessageId::kListConversationsRequest:
    case common::net::MessageId::kGetChatHistoryRequest:
        return *social_upstream_;
    default:
        return *auth_upstream_;
    }
}

}  // namespace services::api_gateway
