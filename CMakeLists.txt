cmake_minimum_required(VERSION 3.16)

project(unity_mmo_backend_skeleton VERSION 0.1.0 LANGUAGES CXX)

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
find_path(IKCP_INCLUDE_DIR ikcp.h REQUIRED)
find_library(IKCP_LIBRARY kcp REQUIRED)

message(STATUS "Using system spdlog")
message(STATUS "Using system KCP")

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
    common/context.proto
    common/envelope.proto
    common/types.proto
    public/auth.proto
    public/gateway.proto
    public/world.proto
    public/scene.proto
    public/combat.proto
    public/instance.proto
    public/player.proto
    public/social.proto
    internal/auth_gateway.proto
    internal/gateway_auth.proto
    internal/gateway_world.proto
    internal/gateway_instance.proto
    internal/gateway_player.proto
    internal/gateway_social.proto
    internal/world_scene.proto
    internal/instance_player.proto
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
    runtime/foundation/server_app.cpp
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
    runtime/protocol/envelope_utils.cpp
    runtime/protocol/internal_auth.cpp
    runtime/protocol/message_router.cpp
)
mmo_defaults(runtime_protocol)
target_link_libraries(runtime_protocol PUBLIC runtime_foundation mmo_protocol)
target_link_libraries(runtime_protocol PUBLIC OpenSSL::Crypto)

add_library(runtime_execution STATIC
    runtime/execution/io_context_pool.cpp
    runtime/execution/sharded_executor.cpp
    runtime/execution/shard_router.cpp
    runtime/execution/scene_worker.cpp
    runtime/execution/instance_shard.cpp
)
mmo_defaults(runtime_execution)
target_link_libraries(runtime_execution PUBLIC Boost::system Threads::Threads)

add_library(runtime_session STATIC
    runtime/session/redis_session_store.cpp
    runtime/session/redis_ticket_replay_store.cpp
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
    runtime/storage/storage_bootstrap.cpp
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
target_link_libraries(runtime_session PUBLIC runtime_storage)

add_library(runtime_transport STATIC
    runtime/transport/connection_session.cpp
    runtime/transport/envelope_codec.cpp
    runtime/transport/envelope_transport.cpp
    runtime/transport/kcp_session.cpp
    runtime/transport/realtime_transport.cpp
    runtime/transport/tcp_envelope_client.cpp
    runtime/transport/tcp_envelope_server.cpp
)
mmo_defaults(runtime_transport)
target_link_libraries(runtime_transport PUBLIC
    runtime_execution
    runtime_protocol
    runtime_observability
    ${IKCP_LIBRARY}
    Boost::system
    Threads::Threads
)
target_include_directories(runtime_transport PUBLIC
    ${IKCP_INCLUDE_DIR}
)

add_library(runtime_channel STATIC
    runtime/channel/channel.cpp
    runtime/channel/channel_client.cpp
    runtime/channel/channel_connection_pool.cpp
    runtime/channel/channel_error.cpp
    runtime/channel/channel_result.cpp
    runtime/channel/routing_policy.cpp
    runtime/channel/service_registry.cpp
    runtime/channel/static_endpoint_resolver.cpp
)
mmo_defaults(runtime_channel)
target_link_libraries(runtime_channel PUBLIC
    runtime_foundation
    runtime_protocol
    runtime_transport
)

add_library(runtime_rpc STATIC
    runtime/rpc/rpc_client.cpp
    runtime/rpc/rpc_error.cpp
    runtime/rpc/rpc_result.cpp
    runtime/rpc/rpc_server_app.cpp
    runtime/rpc/rpc_server.cpp
)
mmo_defaults(runtime_rpc)
target_link_libraries(runtime_rpc PUBLIC
    runtime_channel
    runtime_foundation
    runtime_observability
    runtime_protocol
)

add_library(runtime_routing STATIC
    runtime/routing/forward_result.cpp
    runtime/routing/gateway_forwarder.cpp
    runtime/routing/gateway_router.cpp
    runtime/routing/pending_response_table.cpp
    runtime/routing/route_table.cpp
)
mmo_defaults(runtime_routing)
target_link_libraries(runtime_routing PUBLIC
    runtime_channel
    runtime_protocol
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
        runtime_transport
        runtime_channel
        runtime_execution
        runtime_session
        runtime_observability
        mmo_protocol
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
target_link_libraries(api_gateway_server PRIVATE runtime_routing)

add_mmo_server(
    game_gateway_server
    apps/game_gateway_server/main.cpp
    apps/game_gateway_server/gateway_proxy_handlers.cpp
    apps/game_gateway_server/gateway_route_table.cpp
    apps/game_gateway_server/gateway_session_handlers.cpp
)
target_link_libraries(game_gateway_server PRIVATE runtime_routing)

add_mmo_server(
    world_server
    apps/world_server/main.cpp
    apps/world_server/world_handlers.cpp
)
target_link_libraries(world_server PRIVATE modules_world runtime_rpc)

add_mmo_server(
    scene_server
    apps/scene_server/main.cpp
    apps/scene_server/scene_handlers.cpp
)
target_link_libraries(scene_server PRIVATE modules_scene runtime_rpc)

add_mmo_server(
    instance_server
    apps/instance_server/main.cpp
    apps/instance_server/instance_handlers.cpp
)
target_link_libraries(instance_server PRIVATE modules_instance modules_player runtime_rpc)

add_mmo_server(
    player_server
    apps/player_server/main.cpp
    apps/player_server/player_handlers.cpp
)
target_link_libraries(player_server PRIVATE modules_player runtime_rpc)

add_mmo_server(
    social_server
    apps/social_server/main.cpp
    apps/social_server/social_handlers.cpp
)
target_link_libraries(social_server PRIVATE modules_social runtime_rpc)

add_executable(mmo_flow_client tools/mmo_flow_client/main.cpp)
mmo_defaults(mmo_flow_client)
target_link_libraries(mmo_flow_client PRIVATE runtime_transport mmo_protocol)

add_executable(internal_auth_probe tools/internal_auth_probe/main.cpp)
mmo_defaults(internal_auth_probe)
target_link_libraries(internal_auth_probe PRIVATE runtime_foundation runtime_transport mmo_protocol)

add_executable(security_governance_probe tools/security_governance_probe/main.cpp)
mmo_defaults(security_governance_probe)
target_link_libraries(security_governance_probe PRIVATE runtime_protocol runtime_session)

add_executable(auth_data_probe tools/auth_data_probe/main.cpp)
mmo_defaults(auth_data_probe)
target_link_libraries(auth_data_probe PRIVATE modules_auth)

add_executable(storage_governance_probe tools/storage_governance_probe/main.cpp)
mmo_defaults(storage_governance_probe)
target_link_libraries(storage_governance_probe PRIVATE modules_player runtime_session)

add_executable(production_runtime_governance_probe
    tools/production_runtime_governance_probe/main.cpp
)
mmo_defaults(production_runtime_governance_probe)
target_link_libraries(production_runtime_governance_probe PRIVATE
    runtime_foundation
    runtime_observability
    runtime_storage
)

add_executable(gateway_session_governance_probe
    tools/gateway_session_governance_probe/main.cpp
)
mmo_defaults(gateway_session_governance_probe)
target_link_libraries(gateway_session_governance_probe PRIVATE
    runtime_foundation
    runtime_session
)

add_executable(cluster_routing_governance_probe
    tools/cluster_routing_governance_probe/main.cpp
)
mmo_defaults(cluster_routing_governance_probe)
target_link_libraries(cluster_routing_governance_probe PRIVATE
    runtime_channel
    runtime_observability
)
