#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <mysql/mysql.h>

namespace mmo::runtime::storage {

struct MysqlConfig {
    std::string host{"127.0.0.1"};
    std::string user;
    std::string password;
    std::string database;
    std::uint16_t port{3306};
};

class MysqlClient {
public:
    MysqlClient();
    MysqlClient(const MysqlClient&) = delete;
    MysqlClient& operator=(const MysqlClient&) = delete;
    ~MysqlClient();

    bool connect(const MysqlConfig& config, std::string* error_message);
    bool begin_transaction(std::string* error_message);
    bool commit(std::string* error_message);
    bool rollback(std::string* error_message);
    bool execute(const std::string& sql, std::string* error_message);
    bool query(
        const std::string& sql,
        std::vector<std::vector<std::string>>* rows,
        std::string* error_message);
    std::string escape_string(const std::string& value) const;
    unsigned int last_error_code() const;
    bool is_connected() const;
    void close();

private:
    MYSQL* handle_{};
    bool connected_{};
};

}  // namespace mmo::runtime::storage
