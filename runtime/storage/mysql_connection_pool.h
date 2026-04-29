#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "runtime/storage/mysql_client.h"

namespace mmo::runtime::storage {

class MysqlConnectionPool {
public:
    using ClientLease = std::shared_ptr<MysqlClient>;

    MysqlConnectionPool(MysqlConfig config, std::size_t pool_size);
    MysqlConnectionPool(const MysqlConnectionPool&) = delete;
    MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;
    ~MysqlConnectionPool();

    bool initialize(std::string* error_message);
    ClientLease acquire();
    std::size_t size() const;
    std::size_t available_count() const;
    void close();

private:
    void release(MysqlClient* client);

    MysqlConfig config_;
    std::size_t pool_size_{};
    std::vector<std::unique_ptr<MysqlClient>> clients_;
    std::queue<MysqlClient*> available_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    bool initialized_{};
    bool closing_{};
};

}  // namespace mmo::runtime::storage
