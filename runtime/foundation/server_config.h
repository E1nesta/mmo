#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace mmo::runtime::foundation {

inline constexpr const char* kConfigPathEnv = "MMO_CONFIG_PATH";
inline constexpr const char* kDefaultConfigPath = "configs/local/server.yaml";

struct NetworkConfig {
    std::string bind_host;
    std::string public_host;
};

struct ServiceConfig {
    std::string host;
    std::uint16_t tcp_port{};
    std::uint16_t udp_kcp_port{};
};

struct TcpTransportConfig {
    std::uint32_t max_envelope_payload_bytes{};
    int timeout_millis{};
    int listen_backlog{};
};

struct KcpTransportConfig {
    int nodelay{};
    int interval_millis{};
    int fast_resend{};
    int disable_congestion_control{};
    int send_window{};
    int receive_window{};
};

struct TransportConfig {
    TcpTransportConfig tcp;
    KcpTransportConfig kcp;
};

struct MysqlConfig {
    std::string host;
    std::uint16_t port{};
    std::string database;
    std::string user;
    std::string password_env;
    int pool_size{};
};

struct RedisConfig {
    std::string host;
    std::uint16_t port{};
    int database{};
    std::string password_env;
    int pool_size{};
};

struct StorageConfig {
    MysqlConfig mysql;
    RedisConfig redis;
};

struct ObservabilityConfig {
    std::string log_level;
    std::string log_format;
};

class ServerConfig {
public:
    std::string environment;
    NetworkConfig network;
    TransportConfig transport;
    StorageConfig storage;
    ObservabilityConfig observability;
    std::unordered_map<std::string, ServiceConfig> services;

    const ServiceConfig& service(const std::string& service_name) const;
};

ServerConfig load_server_config(const std::string& path);
ServerConfig load_server_config_from_env();

}  // namespace mmo::runtime::foundation
