#include "runtime/storage/mysql_client.h"

#include <utility>

namespace mmo::runtime::storage {

MysqlClient::MysqlClient() {
    handle_ = mysql_init(nullptr);
}

MysqlClient::~MysqlClient() {
    close();
}

bool MysqlClient::connect(const MysqlConfig& config, std::string* error_message) {
    if (handle_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql_init failed";
        }
        return false;
    }

    if (mysql_real_connect(
            handle_,
            config.host.c_str(),
            config.user.c_str(),
            config.password.c_str(),
            config.database.c_str(),
            config.port,
            nullptr,
            0) == nullptr) {
        if (error_message != nullptr) {
            *error_message = mysql_error(handle_);
        }
        return false;
    }

    connected_ = true;
    return true;
}

bool MysqlClient::execute(const std::string& sql, std::string* error_message) {
    if (!connected_) {
        if (error_message != nullptr) {
            *error_message = "mysql client is not connected";
        }
        return false;
    }

    if (mysql_query(handle_, sql.c_str()) != 0) {
        if (error_message != nullptr) {
            *error_message = mysql_error(handle_);
        }
        return false;
    }
    return true;
}

bool MysqlClient::query(
    const std::string& sql,
    std::vector<std::vector<std::string>>* rows,
    std::string* error_message) {
    if (!connected_) {
        if (error_message != nullptr) {
            *error_message = "mysql client is not connected";
        }
        return false;
    }
    if (rows == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql query rows output is null";
        }
        return false;
    }
    rows->clear();

    if (mysql_query(handle_, sql.c_str()) != 0) {
        if (error_message != nullptr) {
            *error_message = mysql_error(handle_);
        }
        return false;
    }

    MYSQL_RES* result = mysql_store_result(handle_);
    if (result == nullptr) {
        if (mysql_field_count(handle_) == 0) {
            return true;
        }
        if (error_message != nullptr) {
            *error_message = mysql_error(handle_);
        }
        return false;
    }

    const unsigned int field_count = mysql_num_fields(result);
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        std::vector<std::string> values;
        values.reserve(field_count);
        for (unsigned int index = 0; index < field_count; ++index) {
            if (row[index] == nullptr) {
                values.emplace_back();
            } else {
                values.emplace_back(row[index], lengths[index]);
            }
        }
        rows->push_back(std::move(values));
    }
    mysql_free_result(result);
    return true;
}

bool MysqlClient::is_connected() const {
    return connected_;
}

void MysqlClient::close() {
    if (handle_ != nullptr) {
        mysql_close(handle_);
        handle_ = nullptr;
    }
    connected_ = false;
}

}  // namespace mmo::runtime::storage
