// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/application/player_query_service.h"
#include "modules/player/application/player_write_service.h"
#include "modules/player/infrastructure/mysql_player_repository.h"
#include "modules/player/infrastructure/redis_player_cache_repository.h"
#include "modules/player/interfaces/grpc/player_write_service_impl.h"
#include "runtime/foundation/config/simple_config.h"
#include "runtime/grpc/server_runner.h"
#include "runtime/storage/mysql/mysql_client_pool.h"
#include "runtime/storage/redis/redis_client_pool.h"

namespace services::player_write {

class PlayerWriteGrpcServerApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerWriteGrpcServerApp(std::string default_service_name, std::string default_config_path);

    int Main(int argc, char* argv[]);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    bool BuildDependencies(std::string* error_message);
    bool StartServer(std::string* error_message);
    void Shutdown();

    std::string default_service_name_;
    std::string default_config_path_;
    common::config::SimpleConfig config_;
    std::unique_ptr<common::mysql::MySqlClientPool> player_writer_mysql_pool_;
    std::unique_ptr<common::redis::RedisClientPool> player_cache_redis_pool_;
    std::unique_ptr<game_server::player::MySqlPlayerRepository> player_repository_;
    std::unique_ptr<game_server::player::RedisPlayerCacheRepository> player_cache_repository_;
    std::unique_ptr<game_server::player::PlayerQueryService> player_query_service_;
    std::unique_ptr<game_server::player::PlayerWriteService> player_write_service_;
    std::unique_ptr<game_server::player::PlayerWriteServiceImpl> grpc_service_;
    std::unique_ptr<framework::grpc::ServerRunner> server_runner_;
};

}  // namespace services::player_write
