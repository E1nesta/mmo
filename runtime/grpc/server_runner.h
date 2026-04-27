// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <atomic>
#include <memory>
#include <string>

namespace framework::grpc {

struct TlsServerCredentialsOptions {
    std::string cert_chain_file;
    std::string private_key_file;
    std::string client_ca_file;
    bool require_client_auth = false;
};

class ServerRunner {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    ServerRunner(std::string host, int port);

    ServerRunner(const ServerRunner&) = delete;
    ServerRunner& operator=(const ServerRunner&) = delete;

    bool Start(::grpc::Service* service, std::string* error_message = nullptr);
    bool Start(::grpc::Service* service,
               const TlsServerCredentialsOptions& tls_options,
               std::string* error_message = nullptr);
    [[nodiscard]] int Port() const;
    void Wait();
    void Shutdown();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    bool StartWithCredentials(::grpc::Service* service,
                              std::shared_ptr<::grpc::ServerCredentials> credentials,
                              std::string* error_message);

    std::string host_;
    int port_ = 0;
    int selected_port_ = 0;
    std::atomic_bool shutdown_requested_{false};
    std::unique_ptr<::grpc::Server> server_;
};

}  // namespace framework::grpc
