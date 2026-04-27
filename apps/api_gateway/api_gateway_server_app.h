// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "apps/api_gateway/forward_executor.h"
#include "runtime/foundation/config/simple_config.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/http/http_router.h"
#include "runtime/http/http_server.h"
#include "runtime/protocol/request_context.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/transport/transport_client.h"

#include <atomic>
#include <memory>

namespace services::api_gateway {

class ApiGatewayServerApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    ApiGatewayServerApp(std::string default_service_name, std::string default_config_path);

    int Main(int argc, char* argv[]);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    bool BuildDependencies(std::string* error_message);
    void RegisterRoutes();
    bool StartServer(std::string* error_message);
    void Shutdown();

    template <typename RequestProto>
    framework::http::HttpResponse ForwardHttpProto(common::net::MessageId message_id,
                                                   const framework::http::HttpRequest& request,
                                                   const std::function<void(RequestProto&, common::net::RequestContext&)>& enrich);
    template <typename ExternalRequestProto,
              typename InternalRequestProto,
              typename InternalResponseProto,
              typename ExternalResponseProto>
    framework::http::HttpResponse ForwardMappedHttpProto(
        common::net::MessageId message_id,
        const framework::http::HttpRequest& request,
        const std::function<void(const ExternalRequestProto&, common::net::RequestContext&, InternalRequestProto&)>& map_request,
        const std::function<void(const ExternalRequestProto&, const InternalResponseProto&, ExternalResponseProto&)>& map_response);

    common::net::RequestContext BuildBaseContext(const framework::http::HttpRequest& request);
    bool RequiresSession(common::net::MessageId message_id) const;
    bool AuthorizeHttpRequest(common::net::MessageId message_id,
                              common::net::RequestContext* context,
                              framework::http::HttpResponse* error_response) const;
    framework::http::HttpResponse BuildProtoErrorResponse(const common::net::RequestContext& context,
                                                          common::error::ErrorCode error_code,
                                                          std::string error_message) const;
    framework::http::HttpResponse BuildTransportErrorResponse(const common::net::RequestContext& context,
                                                              framework::transport::TransportFailureCode failure_code,
                                                              std::string error_message) const;

    // 运行配置与入口路由：维护网关进程级上下文与 HTTP 路由表。
    std::string default_service_name_;
    std::string default_config_path_;
    common::config::SimpleConfig config_;
    framework::http::HttpRouter router_;
    std::unique_ptr<framework::http::HttpServer> server_;

    // 上游与会话依赖：承接对内转发与会话校验边界。
    std::unique_ptr<ApiGatewayForwardExecutor> forward_executor_;
    std::unique_ptr<common::redis::RedisClientPool> session_redis_pool_;
    std::unique_ptr<common::session::RedisSessionStore> session_store_;

    // 本地运行态：为 HTTP 请求生成单调 request_id。
    std::atomic<std::uint64_t> next_request_id_{1};
};

}  // namespace services::api_gateway
