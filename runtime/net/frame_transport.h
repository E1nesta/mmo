#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace runtime::net {

inline constexpr std::uint32_t kDefaultMaxFramePayloadBytes = 1024 * 1024;
inline constexpr int kDefaultTransportTimeoutMillis = 3000;
inline constexpr std::uint32_t kDefaultMaxConnections = 4096;
inline constexpr std::uint32_t kDefaultMaxWriteQueueDepth = 1024;
inline constexpr std::uint32_t kDefaultMaxInflightFramesPerConnection = 128;
inline constexpr std::uint32_t kDefaultIoThreadCount = 1;
inline constexpr std::uint32_t kDefaultHandlerShardCount = 1;
inline constexpr std::uint32_t kDefaultMaxHandlerQueueDepth = 1024;

struct TransportOptions {
    std::uint32_t max_payload_bytes{kDefaultMaxFramePayloadBytes};
    int timeout_millis{kDefaultTransportTimeoutMillis};
    int listen_backlog{64};
    std::uint32_t max_connections{kDefaultMaxConnections};
    std::uint32_t max_write_queue_depth{kDefaultMaxWriteQueueDepth};
    std::uint32_t max_inflight_frames_per_connection{
        kDefaultMaxInflightFramesPerConnection};
    std::uint32_t io_thread_count{kDefaultIoThreadCount};
    std::uint32_t handler_shard_count{kDefaultHandlerShardCount};
    std::uint32_t max_handler_queue_depth{kDefaultMaxHandlerQueueDepth};
};

struct TransportEndpoint {
    std::string host;
    std::uint16_t port{};
};

enum class TcpCloseReason {
    kNone,
    kPeerClosed,
    kConnectionLimit,
    kFrameTooLarge,
    kReadFailed,
    kDecodeFailed,
    kEncodeFailed,
    kWriteQueueFull,
    kWriteFailed,
    kHandlerQueueFull,
    kHandlerFailed,
    kHandlerStopped,
};

struct TransportStats {
    std::uint64_t active_connections{};
    std::uint64_t accepted_connections{};
    std::uint64_t rejected_connections{};
    std::uint64_t closed_connections{};
    std::uint64_t read_frames{};
    std::uint64_t written_frames{};
    std::uint64_t read_bytes{};
    std::uint64_t written_bytes{};
    std::uint64_t errors{};
};

struct TransportCounters {
    std::atomic<std::uint64_t> active_connections{};
    std::atomic<std::uint64_t> accepted_connections{};
    std::atomic<std::uint64_t> rejected_connections{};
    std::atomic<std::uint64_t> closed_connections{};
    std::atomic<std::uint64_t> read_frames{};
    std::atomic<std::uint64_t> written_frames{};
    std::atomic<std::uint64_t> read_bytes{};
    std::atomic<std::uint64_t> written_bytes{};
    std::atomic<std::uint64_t> errors{};
};

TransportStats snapshot_transport_counters(const TransportCounters& counters);

}  // namespace runtime::net
