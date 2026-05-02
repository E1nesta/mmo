#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "runtime/channel/routing_policy.h"
#include "runtime/channel/service_registry.h"
#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/observability/metrics_exporter.h"

namespace {

mmo::runtime::channel::ServiceInstance make_instance(
    const std::string& service_name,
    const std::string& instance_id,
    std::uint16_t port) {
    mmo::runtime::channel::ServiceInstance instance;
    instance.service_name = service_name;
    instance.instance_id = instance_id;
    instance.endpoint.host = "127.0.0.1";
    instance.endpoint.port = port;
    instance.zone = "local-a";
    instance.weight = 100;
    instance.state = mmo::runtime::channel::ServiceInstanceState::kHealthy;
    return instance;
}

void verify_static_registry() {
    mmo::runtime::channel::StaticServiceRegistry registry({
        make_instance("player_server", "player-1", 5101),
        make_instance("player_server", "player-2", 5102),
    });

    const auto instances = registry.list_instances("player_server");
    assert(instances.size() == 2);
    assert(instances[0].service_name == "player_server");
    assert(registry.find_instance("player_server", "player-2").has_value());

    mmo::runtime::foundation::ServerConfig missing_instance_config;
    missing_instance_config.services.emplace(
        "missing_instances",
        mmo::runtime::foundation::ServiceConfig{});
    bool missing_failed = false;
    try {
        mmo::runtime::channel::StaticServiceRegistry missing(
            missing_instance_config);
    } catch (const std::runtime_error&) {
        missing_failed = true;
    }
    assert(missing_failed);

    mmo::runtime::foundation::ServerConfig duplicate_config;
    mmo::runtime::foundation::ServiceConfig service_config;
    mmo::runtime::foundation::ServiceInstanceConfig first;
    first.instance_id = "dup-1";
    first.host = "127.0.0.1";
    first.tcp_port = 5101;
    mmo::runtime::foundation::ServiceInstanceConfig second = first;
    second.tcp_port = 5102;
    service_config.instances = {first, second};
    duplicate_config.services.emplace("duplicate_instances", service_config);

    bool duplicate_failed = false;
    try {
        mmo::runtime::channel::StaticServiceRegistry duplicate(
            duplicate_config);
    } catch (const std::runtime_error&) {
        duplicate_failed = true;
    }
    assert(duplicate_failed);
}

void verify_routing_policies() {
    std::vector<mmo::runtime::channel::ServiceInstance> instances = {
        make_instance("player_server", "player-1", 5101),
        make_instance("player_server", "player-2", 5102),
    };

    mmo::runtime::channel::ServiceInstanceSelector selector;

    mmo::runtime::channel::RouteSelectionContext round_robin;
    round_robin.target_service = "player_server";
    round_robin.policy = mmo::runtime::channel::RoutingPolicy::kRoundRobin;
    round_robin.request_id = 1;
    const auto first = selector.select(instances, round_robin);
    assert(first.ok());
    const auto second = selector.select(instances, round_robin);
    assert(second.ok());
    assert(first.instance.instance_id != second.instance.instance_id);

    mmo::runtime::channel::RouteSelectionContext player_sticky;
    player_sticky.target_service = "player_server";
    player_sticky.policy = mmo::runtime::channel::RoutingPolicy::kStickyPlayer;
    player_sticky.player_id = 1198216;
    const auto sticky_a = selector.select(instances, player_sticky);
    const auto sticky_b = selector.select(instances, player_sticky);
    assert(sticky_a.ok());
    assert(sticky_b.ok());
    assert(sticky_a.instance.instance_id == sticky_b.instance.instance_id);

    mmo::runtime::channel::RouteSelectionContext instance_sticky;
    instance_sticky.target_service = "instance_server";
    instance_sticky.policy = mmo::runtime::channel::RoutingPolicy::kStickyInstance;
    instance_sticky.route_key = "dungeon-500000";
    const auto instance_a = selector.select(instances, instance_sticky);
    const auto instance_b = selector.select(instances, instance_sticky);
    assert(instance_a.ok());
    assert(instance_b.ok());
    assert(instance_a.instance.instance_id == instance_b.instance.instance_id);

    mmo::runtime::channel::RouteSelectionContext explicit_instance;
    explicit_instance.target_service = "player_server";
    explicit_instance.policy =
        mmo::runtime::channel::RoutingPolicy::kExplicitInstance;
    explicit_instance.target_instance_id = "player-2";
    const auto explicit_result = selector.select(instances, explicit_instance);
    assert(explicit_result.ok());
    assert(explicit_result.instance.instance_id == "player-2");
}

void verify_health_and_pending() {
    mmo::runtime::channel::StaticServiceRegistry registry({
        make_instance("player_server", "player-1", 5101),
        make_instance("player_server", "player-2", 5102),
    });
    registry.mark_unhealthy(
        "player_server",
        "player-1",
        "probe_failure",
        std::chrono::milliseconds(60000));

    mmo::runtime::channel::ServiceInstanceSelector selector;
    mmo::runtime::channel::RouteSelectionContext context;
    context.target_service = "player_server";
    context.policy = mmo::runtime::channel::RoutingPolicy::kRoundRobin;

    const auto selected = selector.select(
        registry.list_instances("player_server"), context);
    assert(selected.ok());
    assert(selected.instance.instance_id == "player-2");

    registry.mark_healthy("player_server", "player-1");
    const auto recovered_instances = registry.list_instances("player_server");
    bool found_healthy_player_1 = false;
    for (const auto& instance : recovered_instances) {
        if (instance.instance_id == "player-1" &&
            instance.state ==
                mmo::runtime::channel::ServiceInstanceState::kHealthy) {
            found_healthy_player_1 = true;
        }
    }
    assert(found_healthy_player_1);

    auto pending_instances = recovered_instances;
    for (auto& instance : pending_instances) {
        if (instance.instance_id == "player-1") {
            instance.pending_count = 128;
        }
    }
    context.max_pending_requests_per_instance = 128;
    const auto pending_selected = selector.select(pending_instances, context);
    assert(pending_selected.ok());
    assert(pending_selected.instance.instance_id == "player-2");
}

void verify_metrics_exporter() {
    mmo::runtime::observability::MetricsRegistry metrics;
    metrics.set_upstream_instance_pending("player_server", "player-1", 3);
    metrics.set_upstream_instance_healthy("player_server", "player-1", true);
    metrics.record_upstream_instance_unhealthy("player_server", "player-1");
    metrics.record_upstream_instance_recovered("player_server", "player-1");
    metrics.record_upstream_instance_circuit_open("player_server", "player-1");
    metrics.record_upstream_instance_request_timeout("player_server", "player-1");
    metrics.record_upstream_instance_remote_error("player_server", "player-1");

    const auto text =
        mmo::runtime::observability::render_prometheus_metrics(metrics.snapshot());
    assert(text.find("mmo_upstream_instance_pending") != std::string::npos);
    assert(text.find("service=\"player_server\"") != std::string::npos);
    assert(text.find("instance_id=\"player-1\"") != std::string::npos);
    assert(text.find("mmo_upstream_instance_unhealthy_total") !=
           std::string::npos);
}

}  // namespace

int main() {
    verify_static_registry();
    verify_routing_policies();
    verify_health_and_pending();
    verify_metrics_exporter();

    std::cout << "cluster routing governance probe ok\n";
    return 0;
}
