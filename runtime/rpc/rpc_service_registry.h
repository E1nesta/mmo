#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/net/frame_transport.h"

namespace runtime::rpc {

enum class RpcServiceInstanceState {
    kHealthy = 0,
    kDraining = 1,
    kUnhealthy = 2,
};

RpcServiceInstanceState parse_service_instance_state(const std::string& value);
std::string service_instance_state_name(RpcServiceInstanceState state);

struct RpcServiceInstance {
    std::string service_name;
    std::string instance_id;
    runtime::net::TransportEndpoint endpoint;
    std::string zone;
    int weight{100};
    RpcServiceInstanceState state{RpcServiceInstanceState::kHealthy};
    std::unordered_map<std::string, std::string> metadata;
    std::size_t pending_count{};
};

class RpcServiceRegistry {
public:
    virtual ~RpcServiceRegistry() = default;

    virtual std::vector<RpcServiceInstance> list_instances(
        const std::string& service_name) const = 0;
    virtual std::optional<RpcServiceInstance> find_instance(
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

class StaticRpcServiceRegistry final : public RpcServiceRegistry {
public:
    explicit StaticRpcServiceRegistry(std::vector<RpcServiceInstance> instances);

    std::vector<RpcServiceInstance> list_instances(
        const std::string& service_name) const override;
    std::optional<RpcServiceInstance> find_instance(
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
        RpcServiceInstanceState state{RpcServiceInstanceState::kHealthy};
        std::chrono::steady_clock::time_point until;
        std::string reason;
    };

    static std::string key(
        const std::string& service_name,
        const std::string& instance_id);
    RpcServiceInstance apply_health_override(RpcServiceInstance instance) const;

    std::unordered_map<std::string, std::vector<RpcServiceInstance>> instances_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, HealthOverride> health_overrides_;
};

}  // namespace runtime::rpc
