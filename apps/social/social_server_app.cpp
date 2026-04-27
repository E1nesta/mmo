// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/social/social_server_app.h"

#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/protocol/adapter_utils.h"

#include "game_backend.pb.h"

namespace services::social {

namespace {

bool RequestPlayerMatchesContext(const framework::protocol::HandlerContext& context, std::int64_t request_player_id) {
    return request_player_id == 0 || request_player_id == context.request.player_id;
}

common::net::Packet BuildListFriendsResponsePacket(
    const framework::protocol::HandlerContext& context,
    const game_server::social::SocialListResponse<game_server::social::FriendSummary>& result) {
    game_backend::proto::ListFriendsResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    for (const auto& item : result.items) {
        auto* friend_item = response.add_friends();
        friend_item->set_player_id(item.player_id);
        friend_item->set_nickname(item.nickname);
        friend_item->set_level(item.level);
        friend_item->set_online(item.online);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kListFriendsResponse, context.request.request_id, response);
}

common::net::Packet BuildListConversationsResponsePacket(
    const framework::protocol::HandlerContext& context,
    const game_server::social::SocialListResponse<game_server::social::ConversationSummary>& result) {
    game_backend::proto::ListConversationsResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    for (const auto& item : result.items) {
        auto* conversation = response.add_conversations();
        conversation->set_conversation_id(item.conversation_id);
        conversation->set_title(item.title);
        conversation->set_unread_count(item.unread_count);
        conversation->set_last_message_preview(item.last_message_preview);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kListConversationsResponse, context.request.request_id, response);
}

common::net::Packet BuildGetChatHistoryResponsePacket(
    const framework::protocol::HandlerContext& context,
    const game_server::social::SocialListResponse<game_server::social::ChatMessage>& result) {
    game_backend::proto::GetChatHistoryResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    for (const auto& item : result.items) {
        auto* message = response.add_messages();
        message->set_message_id(item.message_id);
        message->set_sender_player_id(item.sender_player_id);
        message->set_content(item.content);
        message->set_sent_at_epoch_ms(item.sent_at_epoch_ms);
    }
    return common::net::BuildPacket(
        common::net::MessageId::kGetChatHistoryResponse, context.request.request_id, response);
}

}  // namespace

SocialServerApp::SocialServerApp() : framework::service::ServiceApp("social_server", "configs/social_server.conf") {}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool SocialServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidateSocialStorageConfig(Config(), error_message)) {
        return false;
    }
    const auto mysql_options = common::mysql::ReadReadWritePoolOptions(Config(), "storage.social.mysql.", 4);
    social_writer_mysql_pool_ = std::make_unique<common::mysql::MySqlClientPool>(
        mysql_options.writer.connection, mysql_options.writer.pool_size);
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        Config(), "storage.session.redis.", "storage.redis.", 4);
    session_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!social_writer_mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!session_redis_pool_->Initialize(error_message)) {
        return false;
    }
    social_repository_ = std::make_unique<game_server::social::InMemorySocialRepository>();
    social_service_ = std::make_unique<game_server::social::SocialService>(*social_repository_);
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void SocialServerApp::RegisterRoutes() {
    Routes().Register(common::net::MessageId::kListFriendsRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleListFriendsRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kListConversationsRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleListConversationsRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kGetChatHistoryRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleGetChatHistoryRequest(context, packet);
                      });
}

// 请求处理：`HandleListFriendsRequest` 承接边界输入并转发到目标链路。
common::net::Packet SocialServerApp::HandleListFriendsRequest(const framework::protocol::HandlerContext& context,
                                                              const common::net::Packet& packet) const {
    game_backend::proto::ListFriendsRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid list friends request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRequestContextInvalid, "player mismatch");
    }
    return BuildListFriendsResponsePacket(context, social_service_->ListFriends(context.request.player_id));
}

// 请求处理：`HandleListConversationsRequest` 承接边界输入并转发到目标链路。
common::net::Packet SocialServerApp::HandleListConversationsRequest(
    const framework::protocol::HandlerContext& context,
    const common::net::Packet& packet) const {
    game_backend::proto::ListConversationsRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid list conversations request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRequestContextInvalid, "player mismatch");
    }
    return BuildListConversationsResponsePacket(
        context, social_service_->ListConversations(context.request.player_id));
}

// 请求处理：`HandleGetChatHistoryRequest` 承接边界输入并转发到目标链路。
common::net::Packet SocialServerApp::HandleGetChatHistoryRequest(const framework::protocol::HandlerContext& context,
                                                                 const common::net::Packet& packet) const {
    game_backend::proto::GetChatHistoryRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid get chat history request", &request, &error_response)) {
        return error_response;
    }
    if (!RequestPlayerMatchesContext(context, request.player_id())) {
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRequestContextInvalid, "player mismatch");
    }
    return BuildGetChatHistoryResponsePacket(
        context, social_service_->GetChatHistory(context.request.player_id, request.conversation_id()));
}

}  // namespace services::social
