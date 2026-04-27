// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/packet.h"
#include "runtime/execution/sharded_request_executor.h"
#include "runtime/transport/service_options.h"
#include "runtime/transport/route_registry.h"
#include "runtime/transport/transport_server.h"

#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace framework::protocol {
struct HandlerContext;
}

namespace framework::service {

// 共享服务运行骨架：统一接入、分发与生命周期管理。
// 服务入口保持薄适配：负责装配依赖、注册路由并下沉业务编排。
// 核心业务编排继续交给应用层服务，避免入口层职责膨胀。
class ServiceApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    using Middleware = std::function<bool(common::net::MessageId,
                                          const framework::transport::TransportInbound&,
                                          framework::protocol::HandlerContext*,
                                          common::net::Packet*)>;

    ServiceApp(std::string default_service_name, std::string default_config_path);
    virtual ~ServiceApp() = default;

    int Main(int argc, char* argv[]);

// 受保护扩展：为派生类保留可控扩展点。
protected:
    // 构建基础设施与存储依赖，确保服务边界清晰。
    virtual bool BuildDependencies(std::string* error_message) = 0;
    // 注册轻量路由适配：完成解析、调用与响应映射。
    virtual void RegisterRoutes() = 0;
    virtual void BuildMiddlewares();
    virtual void OnDisconnect(std::uint64_t /*connection_id*/) {}
    virtual bool RequiresTrustedGateway() const { return false; }
    virtual bool SignsTrustedGatewayRequests() const { return false; }
    virtual bool UsesRequestExecutor() const { return true; }
    virtual void DispatchRequest(common::net::MessageId message_id,
                                 const framework::transport::TransportInbound& inbound,
                                 const framework::protocol::HandlerContext& context,
                                 framework::transport::ResponseCallback response_callback);
    virtual std::string DescribeDispatchTarget(common::net::MessageId message_id,
                                               const framework::protocol::HandlerContext& context) const;
    virtual void StopServiceExecutorsAccepting();
    virtual bool WaitForServiceExecutors(std::chrono::milliseconds timeout);
    virtual void ShutdownServiceExecutors();
    void LogHandlerContext(const framework::protocol::HandlerContext& context) const;
    bool SendPacketToConnection(std::uint64_t connection_id, const common::net::Packet& packet) const;
    bool DisconnectConnection(std::uint64_t connection_id) const;

    void AddMiddleware(Middleware middleware);
    Middleware BuildPingMiddleware();
    Middleware BuildContextEnrichmentMiddleware();
    Middleware BuildTrustedGatewayValidationMiddleware();
    Middleware BuildContextValidationMiddleware();
    Middleware BuildLoggingMiddleware();

    [[nodiscard]] common::config::SimpleConfig& Config();
    [[nodiscard]] const common::config::SimpleConfig& Config() const;
    [[nodiscard]] RouteRegistry& Routes();
    [[nodiscard]] const framework::runtime::ServiceCliOptions& Options() const;
    [[nodiscard]] std::string TracePrefix() const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    void HandlePacket(const framework::transport::TransportInbound& inbound,
                      framework::transport::ResponseCallback response_callback);
    framework::protocol::HandlerContext BuildFallbackContext(const framework::transport::TransportInbound& inbound) const;
    framework::transport::TransportServer::Options BuildTransportOptions() const;
    [[nodiscard]] std::chrono::milliseconds ShutdownGracePeriod() const;
    bool ValidateBaseConfig(std::string* error_message) const;
    bool BuildRequestExecutor(std::string* error_message);

    // 服务启动元信息：统一管理默认服务名、配置路径与 CLI 选项。
    std::string default_service_name_;
    std::string default_config_path_;
    framework::runtime::ServiceCliOptions options_;

    // 运行期注册表：配置、路由与中间件链在这里完成收敛。
    common::config::SimpleConfig config_;
    RouteRegistry routes_;
    std::vector<Middleware> middlewares_;

    // 运行执行组件：传输入口与执行器统一由骨架生命周期托管。
    std::unique_ptr<framework::transport::TransportServer> server_;
    std::unique_ptr<framework::execution::ShardedRequestExecutor> request_executor_;
};

}  // namespace framework::service
