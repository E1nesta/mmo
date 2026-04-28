#include "runtime/foundation/server_config.h"

#include <cstdlib>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace mmo::runtime::foundation {
namespace {

std::string env_value(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error("missing required environment variable: " + name);
    }
    return value;
}

std::string read_string(const YAML::Node& node, const std::string& key) {
    if (node[key]) {
        return node[key].as<std::string>();
    }
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        return env_value(node[env_key].as<std::string>());
    }
    throw std::runtime_error("missing config field: " + key);
}

std::string read_optional_string(
    const YAML::Node& node,
    const std::string& key,
    const std::string& default_value = {}) {
    if (node[key]) {
        return node[key].as<std::string>();
    }
    return default_value;
}

int read_int(const YAML::Node& node, const std::string& key) {
    if (node[key]) {
        return node[key].as<int>();
    }
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        return std::stoi(env_value(node[env_key].as<std::string>()));
    }
    throw std::runtime_error("missing config field: " + key);
}

std::uint16_t read_port(const YAML::Node& node, const std::string& key) {
    const int value = read_int(node, key);
    if (value <= 0 || value > 65535) {
        throw std::runtime_error("invalid port for config field: " + key);
    }
    return static_cast<std::uint16_t>(value);
}

std::uint32_t read_uint32(const YAML::Node& node, const std::string& key) {
    const int value = read_int(node, key);
    if (value <= 0) {
        throw std::runtime_error("invalid positive integer for config field: " + key);
    }
    return static_cast<std::uint32_t>(value);
}

ServiceConfig read_service(const YAML::Node& node) {
    ServiceConfig config;
    config.host = read_string(node, "host");
    config.tcp_port = read_port(node, "tcp_port");
    if (node["udp_kcp_port"] || node["udp_kcp_port_env"]) {
        config.udp_kcp_port = read_port(node, "udp_kcp_port");
    }
    return config;
}

MysqlConfig read_mysql(const YAML::Node& node) {
    MysqlConfig config;
    config.host = read_string(node, "host");
    config.port = read_port(node, "port");
    config.database = read_string(node, "database");
    config.user = read_string(node, "user");
    config.password_env = read_optional_string(node, "password_env");
    config.pool_size = read_int(node, "pool_size");
    return config;
}

RedisConfig read_redis(const YAML::Node& node) {
    RedisConfig config;
    config.host = read_string(node, "host");
    config.port = read_port(node, "port");
    config.database = read_int(node, "database");
    config.password_env = read_optional_string(node, "password_env");
    config.pool_size = read_int(node, "pool_size");
    return config;
}

}  // namespace

const ServiceConfig& ServerConfig::service(const std::string& service_name) const {
    const auto it = services.find(service_name);
    if (it == services.end()) {
        throw std::runtime_error("missing service config: " + service_name);
    }
    return it->second;
}

ServerConfig load_server_config(const std::string& path) {
    const YAML::Node root = YAML::LoadFile(path);

    ServerConfig config;
    config.environment = read_string(root, "environment");

    const YAML::Node network = root["network"];
    config.network.bind_host = read_string(network, "bind_host");
    config.network.public_host = read_string(network, "public_host");

    const YAML::Node services = root["services"];
    for (const auto& entry : services) {
        config.services.emplace(
            entry.first.as<std::string>(),
            read_service(entry.second));
    }

    const YAML::Node tcp = root["transport"]["tcp"];
    config.transport.tcp.max_envelope_payload_bytes =
        read_uint32(tcp, "max_envelope_payload_bytes");
    config.transport.tcp.timeout_millis = read_int(tcp, "timeout_millis");
    config.transport.tcp.listen_backlog = read_int(tcp, "listen_backlog");

    const YAML::Node kcp = root["transport"]["kcp"];
    config.transport.kcp.nodelay = read_int(kcp, "nodelay");
    config.transport.kcp.interval_millis = read_int(kcp, "interval_millis");
    config.transport.kcp.fast_resend = read_int(kcp, "fast_resend");
    config.transport.kcp.disable_congestion_control =
        read_int(kcp, "disable_congestion_control");
    config.transport.kcp.send_window = read_int(kcp, "send_window");
    config.transport.kcp.receive_window = read_int(kcp, "receive_window");

    config.storage.mysql = read_mysql(root["storage"]["mysql"]);
    config.storage.redis = read_redis(root["storage"]["redis"]);

    const YAML::Node observability = root["observability"];
    config.observability.log_level = read_string(observability, "log_level");
    config.observability.log_format = read_string(observability, "log_format");

    return config;
}

ServerConfig load_server_config_from_env() {
    const char* path = std::getenv(kConfigPathEnv);
    if (path != nullptr && *path != '\0') {
        return load_server_config(path);
    }
    return load_server_config(kDefaultConfigPath);
}

}  // namespace mmo::runtime::foundation
