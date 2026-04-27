cmake_minimum_required(VERSION 3.16)

project(mobile_game_backend VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(gRPC REQUIRED)
pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(OPENSSL REQUIRED openssl)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

message(STATUS "Using system gRPC/Protobuf toolchain")

function(generate_proto_sources proto_relative_path out_srcs out_hdrs)
    get_filename_component(proto_name "${proto_relative_path}" NAME_WE)
    get_filename_component(proto_dir "${proto_relative_path}" DIRECTORY)
    if(proto_dir)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
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

function(generate_grpc_sources proto_relative_path out_srcs out_hdrs)
    set(additional_deps ${ARGN})
    get_filename_component(proto_name "${proto_relative_path}" NAME_WE)
    get_filename_component(proto_dir "${proto_relative_path}" DIRECTORY)
    if(proto_dir)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${proto_dir}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(generated_src "${output_dir}/${proto_name}.grpc.pb.cc")
    set(generated_hdr "${output_dir}/${proto_name}.grpc.pb.h")
    add_custom_command(
        OUTPUT "${generated_src}" "${generated_hdr}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
        COMMAND "${PROTOC_EXECUTABLE}"
                --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
                --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
                --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                "${CMAKE_CURRENT_SOURCE_DIR}/proto/${proto_relative_path}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/proto/${proto_relative_path}"
            ${additional_deps}
        VERBATIM
    )
    set(${out_srcs} "${generated_src}" PARENT_SCOPE)
    set(${out_hdrs} "${generated_hdr}" PARENT_SCOPE)
endfunction()

generate_proto_sources("game_backend.proto" PROTO_SRCS PROTO_HDRS)
generate_proto_sources("internal/player_internal.proto" INTERNAL_PROTO_SRCS INTERNAL_PROTO_HDRS)
generate_grpc_sources(
    "internal/player_internal.proto"
    INTERNAL_GRPC_SRCS
    INTERNAL_GRPC_HDRS
    "${INTERNAL_PROTO_SRCS}"
    "${INTERNAL_PROTO_HDRS}"
)

add_library(protocol STATIC
    ${PROTO_SRCS}
)

target_link_libraries(protocol
    PUBLIC
        protobuf::libprotobuf
)

target_include_directories(protocol
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(internal_protocol STATIC
    ${INTERNAL_PROTO_SRCS}
    ${INTERNAL_GRPC_SRCS}
)
target_link_libraries(internal_protocol
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)
target_include_directories(internal_protocol
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

function(set_backend_target_defaults target)
    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
            ${MYSQLCLIENT_INCLUDE_DIRS}
            ${HIREDIS_INCLUDE_DIRS}
            ${OPENSSL_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
    )
    target_link_directories(${target}
        PUBLIC
            ${MYSQLCLIENT_LIBRARY_DIRS}
            ${HIREDIS_LIBRARY_DIRS}
            ${OPENSSL_LIBRARY_DIRS}
    )
    target_compile_features(${target} PUBLIC cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic ${MYSQLCLIENT_CFLAGS_OTHER} ${HIREDIS_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})
    endif()
endfunction()

add_library(runtime_foundation STATIC
    runtime/foundation/config/simple_config.cpp
    runtime/foundation/config/storage_boundary_validation.cpp
    runtime/foundation/id/id_generator.cpp
    runtime/foundation/log/logger.cpp
    runtime/foundation/security/password_hasher.cpp
    runtime/foundation/security/session_token.cpp
)
set_backend_target_defaults(runtime_foundation)
target_link_libraries(runtime_foundation
    PUBLIC
        ${OPENSSL_LIBRARIES}
)
target_compile_definitions(runtime_foundation PUBLIC MOBILE_GAME_BACKEND_VERSION="${PROJECT_VERSION}")

add_library(runtime_storage STATIC
    runtime/storage/mysql/mysql_client.cpp
    runtime/storage/mysql/mysql_client_pool.cpp
    runtime/storage/mysql/mysql_read_write_client_pool.cpp
    runtime/storage/redis/redis_client.cpp
    runtime/storage/redis/redis_client_pool.cpp
)
set_backend_target_defaults(runtime_storage)
target_link_libraries(runtime_storage
    PUBLIC
        runtime_foundation
        ${MYSQLCLIENT_LIBRARIES}
        ${HIREDIS_LIBRARIES}
)

add_library(runtime_session STATIC
    runtime/session/in_memory_session_store.cpp
    runtime/session/redis_session_store.cpp
)
set_backend_target_defaults(runtime_session)
target_link_libraries(runtime_session
    PUBLIC
        runtime_foundation
        runtime_storage
)

add_library(runtime_protocol STATIC
    runtime/protocol/packet.cpp
    runtime/protocol/proto_mapper.cpp
    runtime/protocol/context_enrichment.cpp
    runtime/protocol/context_extractor.cpp
    runtime/protocol/error_responder.cpp
    runtime/protocol/message_policy_registry.cpp
    runtime/protocol/packet_codec.cpp
    runtime/protocol/request_dispatcher.cpp
)
set_backend_target_defaults(runtime_protocol)
target_link_libraries(runtime_protocol
    PUBLIC
        protocol
        runtime_foundation
        runtime_session
)

add_library(runtime_observability STATIC
    runtime/observability/structured_log.cpp
)
set_backend_target_defaults(runtime_observability)
target_link_libraries(runtime_observability
    PUBLIC
        runtime_foundation
        runtime_protocol
)

add_library(runtime_execution STATIC
    runtime/execution/execution_types.cpp
    runtime/execution/sharded_request_executor.cpp
)
set_backend_target_defaults(runtime_execution)
target_link_libraries(runtime_execution
    PUBLIC
        runtime_foundation
        runtime_observability
        runtime_protocol
)

add_library(runtime_transport STATIC
    runtime/transport/proxy_protocol.cpp
    runtime/transport/service_options.cpp
    runtime/transport/service_app.cpp
    runtime/transport/transport_server.cpp
    runtime/transport/transport_client.cpp
    runtime/transport/tls_options.cpp
)
set_backend_target_defaults(runtime_transport)
target_link_libraries(runtime_transport
    PUBLIC
        runtime_foundation
        runtime_observability
        runtime_protocol
        runtime_execution
        runtime_session
        runtime_storage
        Boost::system
        Threads::Threads
        ${OPENSSL_LIBRARIES}
)

add_library(runtime_grpc STATIC
    runtime/grpc/channel_factory.cpp
    runtime/grpc/server_runner.cpp
)
set_backend_target_defaults(runtime_grpc)
target_link_libraries(runtime_grpc
    PUBLIC
        runtime_foundation
        runtime_storage
        protobuf::libprotobuf
        gRPC::grpc++
)

add_library(runtime_http STATIC
    runtime/http/http_router.cpp
    runtime/http/http_server.cpp
)
set_backend_target_defaults(runtime_http)
target_link_libraries(runtime_http
    PUBLIC
        runtime_foundation
        runtime_observability
        Boost::system
        Threads::Threads
)

add_library(runtime_core INTERFACE)
target_link_libraries(runtime_core
    INTERFACE
        runtime_foundation
        runtime_storage
        runtime_session
        runtime_observability
        runtime_protocol
        runtime_execution
        runtime_transport
        runtime_grpc
        runtime_http
)
target_compile_definitions(runtime_core INTERFACE MOBILE_GAME_BACKEND_VERSION="${PROJECT_VERSION}")

add_library(login_application STATIC
    modules/login/application/login_service.cpp
)
set_backend_target_defaults(login_application)
target_link_libraries(login_application
    PUBLIC
        runtime_foundation
        runtime_session
)

add_library(login_infrastructure STATIC
    modules/login/infrastructure/in_memory_account_repository.cpp
    modules/login/infrastructure/mysql_account_repository.cpp
)
set_backend_target_defaults(login_infrastructure)
target_link_libraries(login_infrastructure
    PUBLIC
        login_application
        runtime_foundation
        runtime_storage
)

add_library(player_application STATIC
    modules/player/application/player_service.cpp
)
set_backend_target_defaults(player_application)
target_link_libraries(player_application
    PUBLIC
        runtime_foundation
)

add_library(player_infrastructure STATIC
    modules/player/infrastructure/in_memory_player_repository.cpp
    modules/player/infrastructure/mysql_player_repository.cpp
    modules/player/infrastructure/read_write_mysql_player_repository.cpp
    modules/player/infrastructure/redis_player_cache_repository.cpp
)
set_backend_target_defaults(player_infrastructure)
target_link_libraries(player_infrastructure
    PUBLIC
        player_application
        runtime_foundation
        runtime_storage
)

add_library(player_query_application STATIC
    modules/player/application/player_query_service.cpp
)
set_backend_target_defaults(player_query_application)
target_link_libraries(player_query_application
    PUBLIC
        player_application
        runtime_foundation
)

add_library(player_write_application STATIC
    modules/player/application/player_write_service.cpp
)
set_backend_target_defaults(player_write_application)
target_link_libraries(player_write_application
    PUBLIC
        player_application
        runtime_foundation
)

add_library(player_write_grpc_interfaces STATIC
    modules/player/interfaces/grpc/player_write_service_impl.cpp
)
set_backend_target_defaults(player_write_grpc_interfaces)
target_link_libraries(player_write_grpc_interfaces
    PUBLIC
        player_query_application
        player_write_application
        player_infrastructure
        internal_protocol
        runtime_grpc
)

add_library(dungeon_runtime_application STATIC
    modules/dungeon_runtime/application/dungeon_runtime_service.cpp
)
set_backend_target_defaults(dungeon_runtime_application)
target_link_libraries(dungeon_runtime_application
    PUBLIC
        OpenSSL::Crypto
        runtime_foundation
)

add_library(dungeon_runtime_infrastructure STATIC
    modules/dungeon_runtime/infrastructure/in_memory_stage_config_repository.cpp
    modules/dungeon_runtime/infrastructure/grpc_player_snapshot_port.cpp
    modules/dungeon_runtime/infrastructure/mysql_dungeon_repository.cpp
    modules/dungeon_runtime/infrastructure/redis_player_lock_repository.cpp
    modules/dungeon_runtime/infrastructure/redis_dungeon_context_repository.cpp
)
set_backend_target_defaults(dungeon_runtime_infrastructure)
target_link_libraries(dungeon_runtime_infrastructure
    PUBLIC
        dungeon_runtime_application
        runtime_foundation
        runtime_storage
        internal_protocol
        runtime_grpc
)

add_library(social_application STATIC
    modules/social/application/social_service.cpp
)
set_backend_target_defaults(social_application)
target_link_libraries(social_application
    PUBLIC
        runtime_foundation
)

add_library(social_infrastructure STATIC
    modules/social/infrastructure/in_memory_social_repository.cpp
)
set_backend_target_defaults(social_infrastructure)
target_link_libraries(social_infrastructure
    PUBLIC
        social_application
        runtime_foundation
)

add_library(gateway_support STATIC
    apps/gateway/session_binding_service.cpp
    apps/gateway/upstream_response_validator.cpp
)
set_backend_target_defaults(gateway_support)
target_link_libraries(gateway_support PUBLIC runtime_core)

add_library(api_gateway_support STATIC
    apps/api_gateway/forward_executor.cpp
)
set_backend_target_defaults(api_gateway_support)
target_link_libraries(api_gateway_support PUBLIC runtime_core)

function(add_backend_executable target)
    add_executable(${target} ${ARGN})
    set_backend_target_defaults(${target})
endfunction()

add_backend_executable(auth_server
    apps/auth/main.cpp
    apps/auth/auth_server_app.cpp
)
target_link_libraries(auth_server PRIVATE login_application login_infrastructure runtime_core)

add_backend_executable(player_query_server
    apps/player_query/main.cpp
    apps/player_query/player_query_server_app.cpp
)
target_link_libraries(player_query_server
    PRIVATE
        player_query_application
        player_infrastructure
        runtime_core
)

add_backend_executable(player_write_grpc_server
    apps/player_write_grpc_server/main.cpp
    apps/player_write_grpc_server/player_write_grpc_server_app.cpp
)
target_link_libraries(player_write_grpc_server
    PRIVATE
        player_query_application
        player_write_application
        player_infrastructure
        player_write_grpc_interfaces
        runtime_core
)

add_backend_executable(dungeon_runtime_server
    apps/dungeon_runtime/main.cpp
    apps/dungeon_runtime/dungeon_runtime_server_app.cpp
)
target_link_libraries(dungeon_runtime_server
    PRIVATE
        dungeon_runtime_application
        dungeon_runtime_infrastructure
        runtime_core
)

add_backend_executable(online_gateway_server
    apps/online_gateway/main.cpp
    apps/online_gateway/online_gateway_server_app.cpp
)
target_link_libraries(online_gateway_server PRIVATE gateway_support runtime_core)

add_backend_executable(api_gateway_server
    apps/api_gateway/main.cpp
    apps/api_gateway/api_gateway_server_app.cpp
)
target_link_libraries(api_gateway_server PRIVATE api_gateway_support runtime_core)

add_backend_executable(social_server
    apps/social/main.cpp
    apps/social/social_server_app.cpp
)
target_link_libraries(social_server PRIVATE social_application social_infrastructure runtime_core)

add_executable(service_check
    tools/service_check/main.cpp
)
set_backend_target_defaults(service_check)
target_link_libraries(service_check PRIVATE runtime_core)

add_executable(password_tool
    tools/password_tool/main.cpp
)
set_backend_target_defaults(password_tool)
target_link_libraries(password_tool PRIVATE runtime_core)

add_executable(demo_client
    tools/demo_client/main.cpp
    tools/demo_support.cpp
)
set_backend_target_defaults(demo_client)
target_link_libraries(demo_client PRIVATE login_infrastructure runtime_core)

add_executable(load_client
    tools/load_client/main.cpp
    tools/demo_support.cpp
)
set_backend_target_defaults(load_client)
target_link_libraries(load_client PRIVATE runtime_core)

add_executable(auth_service_test
    tests/login_service_test.cpp
)
set_backend_target_defaults(auth_service_test)
target_link_libraries(auth_service_test PRIVATE login_application login_infrastructure runtime_core)

add_executable(player_query_service_test
    tests/player_query_service_test.cpp
)
set_backend_target_defaults(player_query_service_test)
target_link_libraries(player_query_service_test PRIVATE player_query_application player_infrastructure runtime_core)

add_executable(player_write_service_test
    tests/player_write_service_test.cpp
)
set_backend_target_defaults(player_write_service_test)
target_link_libraries(player_write_service_test PRIVATE player_write_application player_infrastructure runtime_core)

add_executable(mysql_rw_config_test
    tests/mysql_rw_config_test.cpp
)
set_backend_target_defaults(mysql_rw_config_test)
target_link_libraries(mysql_rw_config_test PRIVATE runtime_core)

add_executable(storage_boundary_validation_test
    tests/storage_boundary_validation_test.cpp
)
set_backend_target_defaults(storage_boundary_validation_test)
target_link_libraries(storage_boundary_validation_test PRIVATE runtime_core)

add_executable(dungeon_runtime_service_test
    tests/dungeon_runtime_service_test.cpp
)
set_backend_target_defaults(dungeon_runtime_service_test)
target_link_libraries(dungeon_runtime_service_test
    PRIVATE
        dungeon_runtime_application
        dungeon_runtime_infrastructure
        player_query_application
        player_write_application
        player_infrastructure
        player_write_grpc_interfaces
        runtime_core
)

add_executable(online_session_binding_test
    tests/session_binding_store_test.cpp
)
set_backend_target_defaults(online_session_binding_test)
target_link_libraries(online_session_binding_test PRIVATE gateway_support runtime_core)

add_executable(api_gateway_http_proto_test
    tests/api_gateway_http_proto_test.cpp
)
set_backend_target_defaults(api_gateway_http_proto_test)
target_link_libraries(api_gateway_http_proto_test PRIVATE runtime_core runtime_http)

add_executable(social_service_stub_test
    tests/social_service_stub_test.cpp
)
set_backend_target_defaults(social_service_stub_test)
target_link_libraries(social_service_stub_test PRIVATE social_application social_infrastructure runtime_core)

add_executable(packet_codec_test
    tests/packet_codec_test.cpp
)
set_backend_target_defaults(packet_codec_test)
target_link_libraries(packet_codec_test PRIVATE runtime_core)

add_executable(request_dispatcher_test
    tests/request_dispatcher_test.cpp
)
set_backend_target_defaults(request_dispatcher_test)
target_link_libraries(request_dispatcher_test PRIVATE runtime_core)

add_executable(sharded_request_executor_test
    tests/sharded_request_executor_test.cpp
)
set_backend_target_defaults(sharded_request_executor_test)
target_link_libraries(sharded_request_executor_test PRIVATE runtime_core)

add_executable(trusted_gateway_signature_test
    tests/trusted_gateway_signature_test.cpp
)
set_backend_target_defaults(trusted_gateway_signature_test)
target_link_libraries(trusted_gateway_signature_test PRIVATE runtime_core)

add_executable(transport_tls_test
    tests/transport_tls_test.cpp
)
set_backend_target_defaults(transport_tls_test)
target_link_libraries(transport_tls_test PRIVATE runtime_core)

add_executable(proxy_protocol_test
    tests/proxy_protocol_test.cpp
)
set_backend_target_defaults(proxy_protocol_test)
target_link_libraries(proxy_protocol_test PRIVATE runtime_core)

add_executable(service_check_tls_config_test
    tests/service_check_tls_config_test.cpp
)
set_backend_target_defaults(service_check_tls_config_test)
target_link_libraries(service_check_tls_config_test PRIVATE runtime_core)
target_compile_definitions(service_check_tls_config_test PRIVATE PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(service_check_modes_test
    tests/service_check_modes_test.cpp
)
set_backend_target_defaults(service_check_modes_test)
target_link_libraries(service_check_modes_test PRIVATE runtime_core)

add_executable(upstream_response_validator_test
    tests/upstream_response_validator_test.cpp
)
set_backend_target_defaults(upstream_response_validator_test)
target_link_libraries(upstream_response_validator_test PRIVATE gateway_support runtime_core)

add_executable(observability_log_test
    tests/observability_log_test.cpp
)
set_backend_target_defaults(observability_log_test)
target_link_libraries(observability_log_test PRIVATE runtime_core)

add_executable(logger_test
    tests/logger_test.cpp
)
set_backend_target_defaults(logger_test)
target_link_libraries(logger_test PRIVATE runtime_core)

if(BUILD_TESTING)
    add_test(
        NAME architecture_guard
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_architecture.py ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME metrics_snapshot_test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/metrics_snapshot_test.py
    )
endif()
add_test(NAME auth_service_test COMMAND auth_service_test)
add_test(NAME player_query_service_test COMMAND player_query_service_test)
add_test(NAME player_write_service_test COMMAND player_write_service_test)
add_test(NAME mysql_rw_config_test COMMAND mysql_rw_config_test)
add_test(NAME storage_boundary_validation_test COMMAND storage_boundary_validation_test)
add_test(NAME dungeon_runtime_service_test COMMAND dungeon_runtime_service_test)
add_test(NAME online_session_binding_test COMMAND online_session_binding_test)
add_test(NAME api_gateway_http_proto_test COMMAND api_gateway_http_proto_test)
add_test(NAME social_service_stub_test COMMAND social_service_stub_test)
add_test(NAME packet_codec_test COMMAND packet_codec_test)
add_test(NAME request_dispatcher_test COMMAND request_dispatcher_test)
add_test(NAME sharded_request_executor_test COMMAND sharded_request_executor_test)
add_test(NAME trusted_gateway_signature_test COMMAND trusted_gateway_signature_test)
add_test(NAME transport_tls_test COMMAND transport_tls_test)
add_test(NAME proxy_protocol_test COMMAND proxy_protocol_test)
add_test(NAME service_check_tls_config_test COMMAND service_check_tls_config_test)
add_test(NAME service_check_modes_test COMMAND service_check_modes_test)
add_test(NAME upstream_response_validator_test COMMAND upstream_response_validator_test)
add_test(NAME observability_log_test COMMAND observability_log_test)
add_test(NAME logger_test COMMAND logger_test)
