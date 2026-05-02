#include "runtime/foundation/server_config.h"

#include <cstdlib>
#include <set>
#include <stdexcept>
#include <utility>

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
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        const char* value = std::getenv(node[env_key].as<std::string>().c_str());
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    if (node[key]) {
        return node[key].as<std::string>();
    }
    if (node[env_key]) {
        return env_value(node[env_key].as<std::string>());
    }
    throw std::runtime_error("missing config field: " + key);
}

std::string read_optional_string(
    const YAML::Node& node,
    const std::string& key,
    const std::string& default_value = {}) {
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        const char* value = std::getenv(node[env_key].as<std::string>().c_str());
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    if (node[key]) {
        return node[key].as<std::string>();
    }
    return default_value;
}

int read_int(const YAML::Node& node, const std::string& key) {
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        const char* value = std::getenv(node[env_key].as<std::string>().c_str());
        if (value != nullptr && *value != '\0') {
            return std::stoi(value);
        }
    }
    if (node[key]) {
        return node[key].as<int>();
    }
    if (node[env_key]) {
        return std::stoi(env_value(node[env_key].as<std::string>()));
    }
    throw std::runtime_error("missing config field: " + key);
}

int read_optional_int(
    const YAML::Node& node,
    const std::string& key,
    int default_value) {
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        const char* value = std::getenv(node[env_key].as<std::string>().c_str());
        if (value != nullptr && *value != '\0') {
            return std::stoi(value);
        }
    }
    if (node[key]) {
        return node[key].as<int>();
    }
    return default_value;
}

std::uint16_t read_port(const YAML::Node& node, const std::string& key) {
    const int value = read_int(node, key);
    if (value <= 0 || value > 65535) {
        throw std::runtime_error("invalid port for config field: " + key);
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t read_optional_port(
    const YAML::Node& node,
    const std::string& key,
    std::uint16_t default_value = 0) {
    const std::string env_key = key + "_env";
    if (node[env_key]) {
        const char* value = std::getenv(node[env_key].as<std::string>().c_str());
        if (value != nullptr && *value != '\0') {
            const int port = std::stoi(value);
            if (port <= 0 || port > 65535) {
                throw std::runtime_error("invalid port for config field: " + key);
            }
            return static_cast<std::uint16_t>(port);
        }
    }
    if (node[key]) {
        const int port = node[key].as<int>();
        if (port <= 0 || port > 65535) {
            throw std::runtime_error("invalid port for config field: " + key);
        }
        return static_cast<std::uint16_t>(port);
    }
    return default_value;
}

std::uint32_t read_uint32(const YAML::Node& node, const std::string& key) {
    const int value = read_int(node, key);
    if (value <= 0) {
        throw std::runtime_error("invalid positive integer for config field: " + key);
    }
    return static_cast<std::uint32_t>(value);
}

ServiceInstanceConfig read_service_instance(
    const std::string& service_name,
    const YAML::Node& node) {
    ServiceInstanceConfig instance;
    instance.instance_id = read_string(node, "instance_id");
    instance.host = read_string(node, "host");
    instance.tcp_port = read_port(node, "tcp_port");
    instance.udp_kcp_port = read_optional_port(node, "udp_kcp_port", 0);
    instance.zone = read_optional_string(node, "zone");
    instance.weight = read_optional_int(node, "weight", 100);
    instance.state = read_optional_string(node, "state", "healthy");
    if (instance.instance_id.empty()) {
        throw std::runtime_error(
            "service instance id must not be empty: " + service_name);
    }
    if (instance.host.empty()) {
        throw std::runtime_error(
            "service instance host must not be empty: " + service_name +
            "/" + instance.instance_id);
    }
    if (instance.weight <= 0) {
        throw std::runtime_error(
            "service instance weight must be greater than zero: " +
            service_name + "/" + instance.instance_id);
    }
    if (node["metadata"]) {
        for (const auto& entry : node["metadata"]) {
            instance.metadata.emplace(
                entry.first.as<std::string>(),
                entry.second.as<std::string>());
        }
    }
    return instance;
}

ServiceConfig read_service(
    const std::string& service_name,
    const YAML::Node& node) {
    ServiceConfig config;
    config.host = read_string(node, "host");
    config.tcp_port = read_port(node, "tcp_port");
    if (node["udp_kcp_port"] || node["udp_kcp_port_env"]) {
        config.udp_kcp_port = read_port(node, "udp_kcp_port");
    }
    const YAML::Node instances = node["instances"];
    if (!instances || !instances.IsSequence() || instances.size() == 0) {
        throw std::runtime_error(
            "service instances must be configured: " + service_name);
    }
    std::set<std::string> instance_ids;
    for (const auto& instance_node : instances) {
        auto instance = read_service_instance(service_name, instance_node);
        if (!instance_ids.insert(instance.instance_id).second) {
            throw std::runtime_error(
                "duplicate service instance id: " + service_name + "/" +
                instance.instance_id);
        }
        config.instances.push_back(std::move(instance));
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

ExecutionConfig read_execution(const YAML::Node& node) {
    ExecutionConfig config;
    config.io_threads = read_int(node, "io_threads");
    config.handler_shards = read_int(node, "handler_shards");
    config.max_handler_queue_depth_per_shard =
        read_optional_int(node, "max_handler_queue_depth_per_shard", 1024);
    config.player_shards = read_int(node, "player_shards");
    config.scene_shards = read_int(node, "scene_shards");
    config.instance_shards = read_int(node, "instance_shards");
    if (config.io_threads <= 0 || config.handler_shards <= 0 ||
        config.max_handler_queue_depth_per_shard <= 0 ||
        config.player_shards <= 0 || config.scene_shards <= 0 ||
        config.instance_shards <= 0) {
        throw std::runtime_error("execution config values must be greater than zero");
    }
    return config;
}

ChannelConfig read_channel(const YAML::Node& node) {
    ChannelConfig config;
    config.connect_timeout_millis = read_int(node, "connect_timeout_millis");
    config.request_timeout_millis = read_int(node, "request_timeout_millis");
    config.connections_per_upstream = read_int(node, "connections_per_upstream");
    config.max_pending_requests_per_connection =
        read_int(node, "max_pending_requests_per_connection");
    config.max_pending_requests_per_upstream =
        read_optional_int(node, "max_pending_requests_per_upstream", 256);
    if (config.connect_timeout_millis <= 0 || config.request_timeout_millis <= 0 ||
        config.connections_per_upstream <= 0 ||
        config.max_pending_requests_per_connection <= 0 ||
        config.max_pending_requests_per_upstream <= 0) {
        throw std::runtime_error("channel config values must be greater than zero");
    }
    return config;
}

SecurityConfig read_security(const YAML::Node& node) {
    SecurityConfig config;
    const YAML::Node internal_auth = node["internal_auth"];
    config.internal_auth.shared_secret = read_string(internal_auth, "shared_secret");
    config.internal_auth.max_clock_skew_millis =
        read_optional_int(internal_auth, "max_clock_skew_millis", 10000);
    if (config.internal_auth.shared_secret.empty()) {
        throw std::runtime_error(
            "security.internal_auth.shared_secret must not be empty");
    }
    if (config.internal_auth.max_clock_skew_millis <= 0) {
        throw std::runtime_error(
            "security.internal_auth.max_clock_skew_millis must be greater than zero");
    }

    const YAML::Node gateway_ticket = node["gateway_ticket"];
    config.gateway_ticket.shared_secret =
        read_optional_string(gateway_ticket, "shared_secret");
    config.gateway_ticket.issuer =
        read_optional_string(gateway_ticket, "issuer", "mmo-auth");
    config.gateway_ticket.access_audience =
        read_optional_string(
            gateway_ticket, "access_audience", "api_gateway_server");
    config.gateway_ticket.gateway_audience =
        read_optional_string(
            gateway_ticket, "gateway_audience", "game_gateway_server");
    config.gateway_ticket.active_key_id =
        read_optional_string(gateway_ticket, "active_key_id", "local-v1");
    config.gateway_ticket.active_shared_secret =
        read_optional_string(gateway_ticket, "active_shared_secret");
    if (config.gateway_ticket.active_shared_secret.empty()) {
        config.gateway_ticket.active_shared_secret =
            config.gateway_ticket.shared_secret;
    }
    config.gateway_ticket.previous_key_id =
        read_optional_string(gateway_ticket, "previous_key_id");
    config.gateway_ticket.previous_shared_secret =
        read_optional_string(gateway_ticket, "previous_shared_secret");
    config.gateway_ticket.previous_key_accept_millis =
        read_optional_int(gateway_ticket, "previous_key_accept_millis", 0);
    config.gateway_ticket.gateway_ticket_ttl_millis =
        read_optional_int(gateway_ticket, "gateway_ticket_ttl_millis", 60000);
    config.gateway_ticket.access_token_ttl_millis =
        read_optional_int(gateway_ticket, "access_token_ttl_millis", 3600000);
    if (config.gateway_ticket.active_shared_secret.empty()) {
        throw std::runtime_error(
            "security.gateway_ticket.active_shared_secret must not be empty");
    }
    config.gateway_ticket.shared_secret =
        config.gateway_ticket.active_shared_secret;
    if (config.gateway_ticket.gateway_ticket_ttl_millis <= 0 ||
        config.gateway_ticket.access_token_ttl_millis <= 0 ||
        config.gateway_ticket.previous_key_accept_millis < 0) {
        throw std::runtime_error(
            "security.gateway_ticket ttl values must be valid");
    }

    const YAML::Node gateway_session = node["gateway_session"];
    config.gateway_session.gateway_id =
        read_optional_string(
            gateway_session, "gateway_id", "game_gateway_server");
    config.gateway_session.game_session_ttl_millis =
        read_optional_int(gateway_session, "game_session_ttl_millis", 1800000);
    config.gateway_session.heartbeat_timeout_millis =
        read_optional_int(gateway_session, "heartbeat_timeout_millis", 30000);
    config.gateway_session.reconnect_ticket_ttl_millis =
        read_optional_int(gateway_session, "reconnect_ticket_ttl_millis", 60000);
    if (config.gateway_session.game_session_ttl_millis <= 0 ||
        config.gateway_session.heartbeat_timeout_millis <= 0 ||
        config.gateway_session.reconnect_ticket_ttl_millis <= 0) {
        throw std::runtime_error(
            "security.gateway_session ttl values must be greater than zero");
    }
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
        const auto service_name = entry.first.as<std::string>();
        config.services.emplace(
            service_name,
            read_service(service_name, entry.second));
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
    config.execution = read_execution(root["execution"]);
    config.channel = read_channel(root["channel"]);
    config.security = read_security(root["security"]);

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
