// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/packet.h"
#include "runtime/transport/tls_options.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace framework::transport {

enum class TransportFailureCode {
    kNone,
    kResolveFailed,
    kConnectFailed,
    kTimeout,
    kTlsSetupFailed,
    kTlsHandshakeFailed,
    kTlsCertificateValidationFailed,
    kWriteFailed,
    kReadFailed,
    kProtocolDecodeFailed,
    kNoUpstreamClients,
};

class TransportClient {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    TransportClient(std::string host, int port, int timeout_ms, TlsOptions tls_options = {});
    ~TransportClient();

    TransportClient(const TransportClient&) = delete;
    TransportClient& operator=(const TransportClient&) = delete;

    bool SendAndReceive(const common::net::Packet& request,
                        common::net::Packet* response,
                        std::string* error_message,
                        TransportFailureCode* failure_code = nullptr);
    void Close();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    bool EnsureConnected(std::string* error_message, TransportFailureCode* failure_code);
    bool Connect(std::string* error_message, TransportFailureCode* failure_code);
    bool WritePacket(const common::net::Packet& request,
                     std::string* error_message,
                     TransportFailureCode* failure_code);
    bool ReadPacket(common::net::Packet* response, std::string* error_message, TransportFailureCode* failure_code);

    struct Impl;
    Impl* impl_ = nullptr;
};

class UpstreamClientPool {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    UpstreamClientPool(std::string host, int port, int timeout_ms, int pool_size, TlsOptions tls_options = {});

    bool SendAndReceive(const common::net::Packet& request,
                        common::net::Packet* response,
                        std::string* error_message,
                        TransportFailureCode* failure_code = nullptr);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    std::vector<std::unique_ptr<TransportClient>> clients_;
    std::atomic<std::size_t> next_index_{0};
};

}  // namespace framework::transport
