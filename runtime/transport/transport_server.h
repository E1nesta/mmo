// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/packet.h"
#include "runtime/transport/tls_options.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace framework::transport {

struct TransportInbound {
    std::uint64_t connection_id = 0;
    common::net::Packet packet;
    std::string peer_address;
};

using ResponseCallback = std::function<void(common::net::Packet)>;
using PacketHandler = std::function<void(const TransportInbound&, ResponseCallback)>;
using DisconnectHandler = std::function<void(std::uint64_t)>;

class TransportServer {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    struct Options {
        std::size_t io_threads = 1;
        int idle_timeout_ms = 30000;
        std::size_t write_queue_limit = 128;
        std::uint32_t max_packet_body_bytes = 4U * 1024U * 1024U;
        int shutdown_grace_ms = 5000;
        bool proxy_protocol_enabled = false;
        TlsOptions tls;
    };

    struct Impl;

    TransportServer();
    explicit TransportServer(Options options);
    ~TransportServer();

    TransportServer(const TransportServer&) = delete;
    TransportServer& operator=(const TransportServer&) = delete;

    bool Start(const std::string& host, int port, std::string* error_message);
    void SetPacketHandler(PacketHandler handler);
    void SetDisconnectHandler(DisconnectHandler handler);
    bool SendToConnection(std::uint64_t connection_id, const common::net::Packet& packet);
    bool CloseConnection(std::uint64_t connection_id);
    int Run(const std::function<bool()>& keep_running, const std::function<void()>& on_stopping = {});
    void Stop(const std::function<void()>& on_stopping = {});

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    class TransportSession;

    void DoAccept();

    Impl* impl_ = nullptr;
};

}  // namespace framework::transport
