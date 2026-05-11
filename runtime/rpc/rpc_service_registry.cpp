#include "runtime/rpc/rpc_service_registry.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace runtime::rpc {

RpcServiceInstanceState parse_service_instance_state(const std::string& value) {
    if (value == "healthy") {
        return RpcServiceInstanceState::kHealthy;
    }
    if (value == "draining") {
        return RpcServiceInstanceState::kDraining;
    }
    if (value == "unhealthy") {
        return RpcServiceInstanceState::kUnhealthy;
    }
    throw std::runtime_error("unknown service instance state: " + value);
}

std::string service_instance_state_name(RpcServiceInstanceState state) {
    switch (state) {
        case RpcServiceInstanceState::kHealthy:
            return "healthy";
        case RpcServiceInstanceState::kDraining:
            return "draining";
        case RpcServiceInstanceState::kUnhealthy:
            return "unhealthy";
    }
    return "unknown";
}

StaticRpcServiceRegistry::StaticRpcServiceRegistry(
    std::vector<RpcServiceInstance> instances) {
    std::set<std::string> instance_ids;
    for (auto& instance : instances) {
        if (!instance_ids.insert(key(instance.service_name, instance.instance_id))
                 .second) {
            throw std::runtime_error(
                "duplicate service instance id: " + instance.service_name + "/" +
                instance.instance_id);
        }
        instances_[instance.service_name].push_back(std::move(instance));
    }
}

std::vector<RpcServiceInstance> StaticRpcServiceRegistry::list_instances(
    const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = instances_.find(service_name);
    if (it == instances_.end()) {
        return {};
    }
    std::vector<RpcServiceInstance> instances;
    instances.reserve(it->second.size());
    for (const auto& instance : it->second) {
        instances.push_back(apply_health_override(instance));
    }
    return instances;
}

std::optional<RpcServiceInstance> StaticRpcServiceRegistry::find_instance(
    const std::string& service_name,
    const std::string& instance_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = instances_.find(service_name);
    if (it == instances_.end()) {
        return std::nullopt;
    }
    for (const auto& instance : it->second) {
        if (instance.instance_id == instance_id) {
            return apply_health_override(instance);
        }
    }
    return std::nullopt;
}

void StaticRpcServiceRegistry::mark_unhealthy(
    const std::string& service_name,
    const std::string& instance_id,
    const std::string& reason,
    std::chrono::milliseconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    HealthOverride override;
    override.state = RpcServiceInstanceState::kUnhealthy;
    override.until = std::chrono::steady_clock::now() + ttl;
    override.reason = reason;
    health_overrides_[key(service_name, instance_id)] = std::move(override);
}

void StaticRpcServiceRegistry::mark_healthy(
    const std::string& service_name,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    health_overrides_.erase(key(service_name, instance_id));
}

std::string StaticRpcServiceRegistry::key(
    const std::string& service_name,
    const std::string& instance_id) {
    return service_name + "/" + instance_id;
}

RpcServiceInstance StaticRpcServiceRegistry::apply_health_override(
    RpcServiceInstance instance) const {
    const auto it = health_overrides_.find(
        key(instance.service_name, instance.instance_id));
    if (it == health_overrides_.end()) {
        return instance;
    }
    if (std::chrono::steady_clock::now() >= it->second.until) {
        return instance;
    }
    instance.state = it->second.state;
    instance.metadata["unhealthy_reason"] = it->second.reason;
    return instance;
}

}  // namespace runtime::rpc
