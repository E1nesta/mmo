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
    public/common.proto
    public/auth.proto
    public/gateway.proto
    public/world.proto
    public/scene.proto
    public/combat.proto
    public/instance.proto
    public/player.proto
    public/social.proto
    internal/auth_gateway.proto
    internal/gateway_world.proto
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
    runtime/foundation/server_config.cpp
)
mmo_defaults(runtime_foundation)
if(TARGET yaml-cpp::yaml-cpp)
    target_link_libraries(runtime_foundation PUBLIC yaml-cpp::yaml-cpp)
else()
    target_link_libraries(runtime_foundation PUBLIC yaml-cpp)
endif()

add_library(runtime_protocol STATIC
    runtime/protocol/envelope_utils.cpp
)
mmo_defaults(runtime_protocol)
target_link_libraries(runtime_protocol PUBLIC runtime_foundation mmo_protocol)

add_library(runtime_execution STATIC
    runtime/execution/sharded_executor.cpp
    runtime/execution/scene_worker.cpp
    runtime/execution/instance_shard.cpp
)
mmo_defaults(runtime_execution)
target_link_libraries(runtime_execution PUBLIC Threads::Threads)

add_library(runtime_session STATIC
    runtime/session/session_context.cpp
)
mmo_defaults(runtime_session)

add_library(runtime_observability STATIC
    runtime/observability/logging.cpp
)
mmo_defaults(runtime_observability)
target_link_libraries(runtime_observability PUBLIC mmo_protocol)
target_link_libraries(runtime_observability PUBLIC spdlog::spdlog)

add_library(runtime_storage STATIC
    runtime/storage/mysql_client.cpp
    runtime/storage/redis_client.cpp
    runtime/storage/redis_keys.cpp
)
mmo_defaults(runtime_storage)
target_include_directories(runtime_storage PUBLIC
    ${MYSQL_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
)
target_link_libraries(runtime_storage PUBLIC
    ${MYSQL_LIBRARY}
    ${HIREDIS_LIBRARY}
)

add_library(runtime_transport STATIC
    runtime/transport/envelope_transport.cpp
    runtime/transport/tcp_envelope_server.cpp
    runtime/transport/udp_kcp_endpoint.cpp
)
mmo_defaults(runtime_transport)
target_link_libraries(runtime_transport PUBLIC
    runtime_protocol
    runtime_observability
    ${IKCP_LIBRARY}
    Boost::system
    Threads::Threads
)
target_include_directories(runtime_transport PUBLIC
    ${IKCP_INCLUDE_DIR}
)

add_library(modules_auth STATIC modules/auth/auth_service.cpp)
mmo_defaults(modules_auth)

add_library(modules_gateway STATIC modules/gateway/gateway_session.cpp)
mmo_defaults(modules_gateway)

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
        runtime_execution
        runtime_session
        runtime_observability
        mmo_protocol
    )
endfunction()

add_mmo_server(auth_server apps/auth_server/main.cpp)
target_link_libraries(auth_server PRIVATE modules_auth)

add_mmo_server(gateway_server apps/gateway_server/main.cpp)
target_link_libraries(gateway_server PRIVATE modules_gateway)

add_mmo_server(world_server apps/world_server/main.cpp)
target_link_libraries(world_server PRIVATE modules_world)

add_mmo_server(scene_server apps/scene_server/main.cpp)
target_link_libraries(scene_server PRIVATE modules_scene)

add_mmo_server(instance_server apps/instance_server/main.cpp)
target_link_libraries(instance_server PRIVATE modules_instance modules_player)

add_mmo_server(player_server apps/player_server/main.cpp)
target_link_libraries(player_server PRIVATE modules_player)

add_mmo_server(social_server apps/social_server/main.cpp)
target_link_libraries(social_server PRIVATE modules_social)

add_executable(mmo_flow_client tools/mmo_flow_client/main.cpp)
mmo_defaults(mmo_flow_client)
target_link_libraries(mmo_flow_client PRIVATE runtime_transport mmo_protocol)
