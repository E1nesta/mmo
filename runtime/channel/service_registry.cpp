#include "runtime/channel/service_registry.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace runtime::channel {
namespace {

ServiceInstance to_service_instance(
    const std::string& service_name,
    const runtime::foundation::ServiceInstanceConfig& config) {
    ServiceInstance instance;
    instance.service_name = service_name;
    instance.instance_id = config.instance_id;
    instance.endpoint.host = config.host;
    instance.endpoint.port = config.tcp_port;
    instance.udp_kcp_port = config.udp_kcp_port;
    instance.zone = config.zone;
    instance.weight = config.weight;
    instance.state = parse_service_instance_state(config.state);
    instance.metadata = config.metadata;
    return instance;
}

}  // namespace

ServiceInstanceState parse_service_instance_state(const std::string& value) {
    if (value == "healthy") {
        return ServiceInstanceState::kHealthy;
    }
    if (value == "draining") {
        return ServiceInstanceState::kDraining;
    }
    if (value == "unhealthy") {
        return ServiceInstanceState::kUnhealthy;
    }
    throw std::runtime_error("unknown service instance state: " + value);
}

std::string service_instance_state_name(ServiceInstanceState state) {
    switch (state) {
        case ServiceInstanceState::kHealthy:
            return "healthy";
        case ServiceInstanceState::kDraining:
            return "draining";
        case ServiceInstanceState::kUnhealthy:
            return "unhealthy";
    }
    return "unknown";
}

StaticServiceRegistry::StaticServiceRegistry(
    const runtime::foundation::ServerConfig& config) {
    for (const auto& [service_name, service_config] : config.services) {
        if (service_config.instances.empty()) {
            throw std::runtime_error(
                "service instances must be configured: " + service_name);
        }
        std::set<std::string> instance_ids;
        auto& instances = instances_[service_name];
        instances.reserve(service_config.instances.size());
        for (const auto& instance_config : service_config.instances) {
            if (!instance_ids.insert(instance_config.instance_id).second) {
                throw std::runtime_error(
                    "duplicate service instance id: " + service_name + "/" +
                    instance_config.instance_id);
            }
            instances.push_back(to_service_instance(service_name, instance_config));
        }
    }
}

StaticServiceRegistry::StaticServiceRegistry(
    std::vector<ServiceInstance> instances) {
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

std::vector<ServiceInstance> StaticServiceRegistry::list_instances(
    const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = instances_.find(service_name);
    if (it == instances_.end()) {
        return {};
    }
    std::vector<ServiceInstance> instances;
    instances.reserve(it->second.size());
    for (const auto& instance : it->second) {
        instances.push_back(apply_health_override(instance));
    }
    return instances;
}

std::optional<ServiceInstance> StaticServiceRegistry::find_instance(
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

void StaticServiceRegistry::mark_unhealthy(
    const std::string& service_name,
    const std::string& instance_id,
    const std::string& reason,
    std::chrono::milliseconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    HealthOverride override;
    override.state = ServiceInstanceState::kUnhealthy;
    override.until = std::chrono::steady_clock::now() + ttl;
    override.reason = reason;
    health_overrides_[key(service_name, instance_id)] = std::move(override);
}

void StaticServiceRegistry::mark_healthy(
    const std::string& service_name,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    health_overrides_.erase(key(service_name, instance_id));
}

std::string StaticServiceRegistry::key(
    const std::string& service_name,
    const std::string& instance_id) {
    return service_name + "/" + instance_id;
}

ServiceInstance StaticServiceRegistry::apply_health_override(
    ServiceInstance instance) const {
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

}  // namespace runtime::channel
