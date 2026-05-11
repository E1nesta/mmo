cmake_minimum_required(VERSION 3.16)

project(unity_mmo_backend_skeleton VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h REQUIRED)
find_library(MYSQL_LIBRARY mysqlclient REQUIRED)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h REQUIRED)
find_library(HIREDIS_LIBRARY hiredis REQUIRED)

message(STATUS "Using system spdlog")

find_program(PROTOC_EXECUTABLE protoc REQUIRED)

function(generate_proto_sources proto_relative_path out_srcs out_hdrs)
    get_filename_component(proto_name "${proto_relative_path}" NAME_WE)
    get_filename_component(proto_dir "${proto_relative_path}" DIRECTORY)
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}")
    set(generated_src "${output_dir}/${proto_name}.pb.cc")
    set(generated_hdr "${output_dir}/${proto_name}.pb.h")

    add_custom_command(
        OUTPUT "${generated_src}" "${generated_hdr}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
        COMMAND "${PROTOC_EXECUTABLE}"
                --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
                --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                "${CMAKE_CURRENT_SOURCE_DIR}/proto/${proto_relative_path}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/proto/${proto_relative_path}"
        VERBATIM
    )

    set(${out_srcs} "${generated_src}" PARENT_SCOPE)
    set(${out_hdrs} "${generated_hdr}" PARENT_SCOPE)
endfunction()

set(PROTO_FILES
    common/error.proto
    common/types.proto
    cs/auth.proto
    cs/gateway.proto
    cs/world.proto
    cs/scene.proto
    cs/combat.proto
    cs/instance.proto
    cs/player.proto
    cs/social.proto
    ss/gateway_auth.proto
    ss/world_scene.proto
    ss/instance_player.proto
)

set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(proto_file IN LISTS PROTO_FILES)
    generate_proto_sources("${proto_file}" generated_src generated_hdr)
    list(APPEND PROTO_SRCS "${generated_src}")
    list(APPEND PROTO_HDRS "${generated_hdr}")
endforeach()

add_library(mmo_protocol STATIC ${PROTO_SRCS})
add_custom_target(generate_mmo_proto DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})
add_dependencies(mmo_protocol generate_mmo_proto)
target_link_libraries(mmo_protocol PUBLIC protobuf::libprotobuf)
target_include_directories(mmo_protocol PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

function(mmo_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(runtime_foundation STATIC
    runtime/foundation/readiness.cpp
    runtime/foundation/server_config.cpp
)
mmo_defaults(runtime_foundation)
if(TARGET yaml-cpp::yaml-cpp)
    target_link_libraries(runtime_foundation PUBLIC yaml-cpp::yaml-cpp)
else()
    target_link_libraries(runtime_foundation PUBLIC yaml-cpp)
endif()

add_library(runtime_protocol STATIC
    runtime/protocol/auth_tokens.cpp
    runtime/protocol/message_catalog.cpp
    runtime/protocol/payload_utils.cpp
)
mmo_defaults(runtime_protocol)
target_link_libraries(runtime_protocol PUBLIC mmo_protocol)
target_link_libraries(runtime_protocol PUBLIC OpenSSL::Crypto)

add_library(runtime_scheduler STATIC
    runtime/scheduler/io_context_pool.cpp
    runtime/scheduler/sharded_executor.cpp
    runtime/scheduler/shard_router.cpp
)
mmo_defaults(runtime_scheduler)
target_link_libraries(runtime_scheduler PUBLIC Boost::system Threads::Threads)

add_library(runtime_session STATIC
    runtime/session/session_context.cpp
    runtime/session/ticket_replay_guard.cpp
)
mmo_defaults(runtime_session)

add_library(runtime_observability STATIC
    runtime/observability/logging.cpp
    runtime/observability/metrics.cpp
    runtime/observability/metrics_exporter.cpp
)
mmo_defaults(runtime_observability)
target_link_libraries(runtime_observability PUBLIC mmo_protocol)
target_link_libraries(runtime_observability PUBLIC spdlog::spdlog)

add_library(runtime_storage STATIC
    runtime/storage/mysql_client.cpp
    runtime/storage/mysql_connection_pool.cpp
    runtime/storage/redis_client.cpp
    runtime/storage/redis_connection_pool.cpp
    runtime/storage/redis_keys.cpp
    runtime/storage/schema_migration.cpp
    runtime/storage/storage_runtime.cpp
)
mmo_defaults(runtime_storage)
target_include_directories(runtime_storage PUBLIC
    ${MYSQL_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
)
target_link_libraries(runtime_storage PUBLIC
    runtime_foundation
    OpenSSL::Crypto
    ${MYSQL_LIBRARY}
    ${HIREDIS_LIBRARY}
)

add_library(adapters_session_redis STATIC
    adapters/session_redis/redis_session_store.cpp
    adapters/session_redis/redis_ticket_replay_store.cpp
)
mmo_defaults(adapters_session_redis)
target_link_libraries(adapters_session_redis PUBLIC
    runtime_session
    runtime_storage
)

add_library(runtime_net STATIC
    runtime/net/rpc_frame_codec.cpp
    runtime/net/frame_transport.cpp
    runtime/net/realtime_packet_codec.cpp
    runtime/net/reliable_frame_transport.cpp
    runtime/net/reliable_frame_codec.cpp
    runtime/net/rpc_frame_transport.cpp
    runtime/net/tcp_listener.cpp
    runtime/net/tcp_session.cpp
)
mmo_defaults(runtime_net)
target_link_libraries(runtime_net PUBLIC
    runtime_scheduler
    runtime_protocol
    Boost::system
    Threads::Threads
)

add_library(runtime_rpc STATIC
    runtime/rpc/rpc_connection.cpp
    runtime/rpc/rpc_connection_pool.cpp
    runtime/rpc/rpc_client.cpp
    runtime/rpc/rpc_dispatcher.cpp
    runtime/rpc/rpc_error.cpp
    runtime/rpc/rpc_pending_tracker.cpp
    runtime/rpc/rpc_result.cpp
    runtime/rpc/rpc_routing_policy.cpp
    runtime/rpc/rpc_service_registry.cpp
)
mmo_defaults(runtime_rpc)
target_link_libraries(runtime_rpc PUBLIC
    runtime_net
    runtime_observability
    runtime_protocol
)

add_library(runtime_entity STATIC
    runtime/entity/entity_executor.cpp
    runtime/entity/entity_id.cpp
    runtime/entity/entity_mailbox.cpp
    runtime/entity/entity_router.cpp
)
mmo_defaults(runtime_entity)
target_link_libraries(runtime_entity PUBLIC runtime_scheduler)

add_library(runtime_scene STATIC
    runtime/scene/scene_fanout_budget.cpp
    runtime/scene/scene_interest.cpp
    runtime/scene/scene_owner.cpp
    runtime/scene/scene_tick.cpp
)
mmo_defaults(runtime_scene)

add_library(runtime_handler INTERFACE)
target_compile_features(runtime_handler INTERFACE cxx_std_17)
target_include_directories(runtime_handler INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(runtime_handler INTERFACE
    runtime_protocol
    runtime_rpc
)

add_library(runtime_service STATIC
    runtime/service/service_app.cpp
    runtime/service/service_runtime.cpp
)
mmo_defaults(runtime_service)
target_link_libraries(runtime_service PUBLIC
    runtime_foundation
    runtime_rpc
    runtime_net
    runtime_handler
    runtime_observability
    runtime_storage
)

add_library(runtime_gateway STATIC
    runtime/gateway/gateway_middleware.cpp
    runtime/gateway/gateway_router.cpp
    runtime/gateway/gateway_session.cpp
    runtime/gateway/gateway_context.cpp
    runtime/gateway/gateway_route_table.cpp
    runtime/gateway/realtime_gateway_handler.cpp
    runtime/gateway/realtime_gateway_route.cpp
)
mmo_defaults(runtime_gateway)
target_link_libraries(runtime_gateway PUBLIC
    runtime_net
    runtime_rpc
    runtime_observability
    runtime_protocol
    runtime_session
)

add_library(modules_auth STATIC
    modules/auth/auth_service.cpp
    modules/auth/mysql_account_repository.cpp
    modules/auth/mysql_player_identity_repository.cpp
    modules/auth/password_hasher.cpp
)
mmo_defaults(modules_auth)
target_link_libraries(modules_auth PUBLIC runtime_storage OpenSSL::Crypto)

add_library(modules_world STATIC modules/world/world_service.cpp)
mmo_defaults(modules_world)

add_library(modules_scene STATIC modules/scene/scene_service.cpp)
mmo_defaults(modules_scene)

add_library(modules_aoi STATIC modules/aoi/aoi_grid.cpp)
mmo_defaults(modules_aoi)

add_library(modules_movement STATIC modules/movement/movement_service.cpp)
mmo_defaults(modules_movement)

add_library(modules_combat STATIC modules/combat/combat_service.cpp)
mmo_defaults(modules_combat)

add_library(modules_numeric STATIC modules/numeric/numeric_config.cpp)
mmo_defaults(modules_numeric)

add_library(modules_instance STATIC modules/instance/instance_service.cpp)
mmo_defaults(modules_instance)

add_library(modules_player STATIC
    modules/player/player_service.cpp
    modules/player/mysql_player_repository.cpp
)
mmo_defaults(modules_player)
target_link_libraries(modules_player PUBLIC runtime_storage)

add_library(modules_social STATIC modules/social/social_boundary.cpp)
mmo_defaults(modules_social)

function(add_mmo_server target)
    add_executable(${target} ${ARGN})
    mmo_defaults(${target})
    target_link_libraries(${target} PRIVATE
        runtime_service
    )
endfunction()

function(add_mmo_probe_test target label)
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${target}
        PROPERTIES
            LABELS "${label}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

add_mmo_server(
    auth_server
    apps/auth_server/main.cpp
    apps/auth_server/auth_handlers.cpp
)
target_link_libraries(auth_server PRIVATE modules_auth runtime_rpc)

add_mmo_server(
    api_gateway_server
    apps/api_gateway_server/main.cpp
    apps/api_gateway_server/api_handler.cpp
    apps/api_gateway_server/api_http_server.cpp
)

add_mmo_server(
    game_gateway_server
    apps/game_gateway_server/main.cpp
    apps/game_gateway_server/gateway_gate_login_handler.cpp
    apps/game_gateway_server/gateway_ping_handler.cpp
    apps/game_gateway_server/gateway_route_handlers.cpp
    apps/game_gateway_server/gateway_reconnect_handler.cpp
    apps/game_gateway_server/gateway_route_table.cpp
    apps/game_gateway_server/gateway_session_handlers.cpp
)
target_link_libraries(game_gateway_server PRIVATE
    runtime_gateway
    adapters_session_redis
)

add_mmo_server(
    realtime_gateway_server
    apps/realtime_gateway_server/main.cpp
    apps/realtime_gateway_server/realtime_route_table.cpp
)
target_link_libraries(realtime_gateway_server PRIVATE
    runtime_gateway
)

add_mmo_server(
    world_server
    apps/world_server/main.cpp
    apps/world_server/world_handlers.cpp
)
target_link_libraries(world_server PRIVATE modules_world runtime_rpc runtime_entity)

add_mmo_server(
    scene_server
    apps/scene_server/main.cpp
    apps/scene_server/scene_handlers.cpp
)
target_link_libraries(scene_server PRIVATE modules_scene runtime_rpc)
target_link_libraries(scene_server PRIVATE runtime_entity)

add_mmo_server(
    instance_server
    apps/instance_server/main.cpp
    apps/instance_server/instance_handlers.cpp
)
target_link_libraries(instance_server PRIVATE
    modules_instance
    modules_player
    runtime_rpc
    runtime_entity
)

add_mmo_server(
    player_server
    apps/player_server/main.cpp
    apps/player_server/player_handlers.cpp
)
target_link_libraries(player_server PRIVATE
    modules_player
    runtime_rpc
    runtime_entity
)

add_mmo_server(
    social_server
    apps/social_server/main.cpp
    apps/social_server/social_handlers.cpp
)
target_link_libraries(social_server PRIVATE
    modules_social
    runtime_rpc
    runtime_entity
)

add_executable(security_governance_probe tools/security_governance_probe/main.cpp)
mmo_defaults(security_governance_probe)
target_link_libraries(security_governance_probe PRIVATE runtime_protocol runtime_session)
add_mmo_probe_test(security_governance_probe local)

add_executable(auth_data_probe tools/auth_data_probe/main.cpp)
mmo_defaults(auth_data_probe)
target_link_libraries(auth_data_probe PRIVATE modules_auth)
add_mmo_probe_test(auth_data_probe integration)

add_executable(storage_governance_probe tools/storage_governance_probe/main.cpp)
mmo_defaults(storage_governance_probe)
target_link_libraries(storage_governance_probe PRIVATE
    modules_player
    runtime_session
    adapters_session_redis
)
add_mmo_probe_test(storage_governance_probe integration)

add_executable(production_runtime_governance_probe
    tools/production_runtime_governance_probe/main.cpp
)
mmo_defaults(production_runtime_governance_probe)
target_link_libraries(production_runtime_governance_probe PRIVATE
    runtime_foundation
    runtime_observability
    runtime_storage
)
add_mmo_probe_test(production_runtime_governance_probe local)

add_executable(gateway_session_governance_probe
    tools/gateway_session_governance_probe/main.cpp
)
mmo_defaults(gateway_session_governance_probe)
target_link_libraries(gateway_session_governance_probe PRIVATE
    runtime_foundation
    runtime_session
    adapters_session_redis
)
add_mmo_probe_test(gateway_session_governance_probe integration)

add_executable(cluster_routing_governance_probe
    tools/cluster_routing_governance_probe/main.cpp
)
mmo_defaults(cluster_routing_governance_probe)
target_link_libraries(cluster_routing_governance_probe PRIVATE
    runtime_rpc
    runtime_observability
)
add_mmo_probe_test(cluster_routing_governance_probe local)

add_executable(protocol_codec_governance_probe
    tools/protocol_codec_governance_probe/main.cpp
)
mmo_defaults(protocol_codec_governance_probe)
target_link_libraries(protocol_codec_governance_probe PRIVATE
    runtime_protocol
    runtime_net
)
add_mmo_probe_test(protocol_codec_governance_probe local)

add_executable(entity_mailbox_governance_probe
    tools/entity_mailbox_governance_probe/main.cpp
)
mmo_defaults(entity_mailbox_governance_probe)
target_link_libraries(entity_mailbox_governance_probe PRIVATE
    runtime_entity
    runtime_scheduler
)
add_mmo_probe_test(entity_mailbox_governance_probe local)

add_executable(gateway_route_governance_probe
    tools/gateway_route_governance_probe/main.cpp
)
mmo_defaults(gateway_route_governance_probe)
target_link_libraries(gateway_route_governance_probe PRIVATE
    runtime_gateway
    runtime_protocol
    runtime_net
    runtime_rpc
    runtime_session
)
add_mmo_probe_test(gateway_route_governance_probe local)

add_executable(scheduler_governance_probe
    tools/scheduler_governance_probe/main.cpp
)
mmo_defaults(scheduler_governance_probe)
target_link_libraries(scheduler_governance_probe PRIVATE
    runtime_scheduler
)
add_mmo_probe_test(scheduler_governance_probe local)

add_executable(rpc_governance_probe
    tools/rpc_governance_probe/main.cpp
)
mmo_defaults(rpc_governance_probe)
target_link_libraries(rpc_governance_probe PRIVATE
    runtime_rpc
)
add_mmo_probe_test(rpc_governance_probe local)
