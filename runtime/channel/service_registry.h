#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/foundation/server_config.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::channel {

enum class ServiceInstanceState {
    kHealthy = 0,
    kDraining = 1,
    kUnhealthy = 2,
};

ServiceInstanceState parse_service_instance_state(const std::string& value);
std::string service_instance_state_name(ServiceInstanceState state);

struct ServiceInstance {
    std::string service_name;
    std::string instance_id;
    mmo::runtime::transport::TransportEndpoint endpoint;
    std::uint16_t udp_kcp_port{};
    std::string zone;
    int weight{100};
    ServiceInstanceState state{ServiceInstanceState::kHealthy};
    std::unordered_map<std::string, std::string> metadata;
    std::size_t pending_count{};
};

class ServiceRegistry {
public:
    virtual ~ServiceRegistry() = default;

    virtual std::vector<ServiceInstance> list_instances(
        const std::string& service_name) const = 0;
    virtual std::optional<ServiceInstance> find_instance(
        const std::string& service_name,
        const std::string& instance_id) const = 0;
    virtual void mark_unhealthy(
        const std::string& service_name,
        const std::string& instance_id,
        const std::string& reason,
        std::chrono::milliseconds ttl) = 0;
    virtual void mark_healthy(
        const std::string& service_name,
        const std::string& instance_id) = 0;
};

class StaticServiceRegistry final : public ServiceRegistry {
public:
    explicit StaticServiceRegistry(
        const mmo::runtime::foundation::ServerConfig& config);
    explicit StaticServiceRegistry(std::vector<ServiceInstance> instances);

    std::vector<ServiceInstance> list_instances(
        const std::string& service_name) const override;
    std::optional<ServiceInstance> find_instance(
        const std::string& service_name,
        const std::string& instance_id) const override;
    void mark_unhealthy(
        const std::string& service_name,
        const std::string& instance_id,
        const std::string& reason,
        std::chrono::milliseconds ttl) override;
    void mark_healthy(
        const std::string& service_name,
        const std::string& instance_id) override;

private:
    struct HealthOverride {
        ServiceInstanceState state{ServiceInstanceState::kHealthy};
        std::chrono::steady_clock::time_point until;
        std::string reason;
    };

    static std::string key(
        const std::string& service_name,
        const std::string& instance_id);
    ServiceInstance apply_health_override(ServiceInstance instance) const;

    std::unordered_map<std::string, std::vector<ServiceInstance>> instances_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, HealthOverride> health_overrides_;
};

}  // namespace mmo::runtime::channel
