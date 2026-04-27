// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/storage/mysql/mysql_read_write_client_pool.h"

namespace common::mysql {

MySqlReadWriteClientPool::MySqlReadWriteClientPool(ReadWritePoolOptions options) {
    writer_pool_ = std::make_unique<MySqlClientPool>(std::move(options.writer.connection), options.writer.pool_size);
    if (options.reader.has_value()) {
        reader_pool_ = std::make_unique<MySqlClientPool>(
            std::move(options.reader->connection), options.reader->pool_size);
    }
}

bool MySqlReadWriteClientPool::Initialize(std::string* error_message) {
    if (writer_pool_ == nullptr || !writer_pool_->Initialize(error_message)) {
        return false;
    }
    if (reader_pool_ != nullptr && !reader_pool_->Initialize(error_message)) {
        return false;
    }
    return true;
}

bool MySqlReadWriteClientPool::HasReader() const {
    return reader_pool_ != nullptr;
}

MySqlClientPool& MySqlReadWriteClientPool::Writer() {
    return *writer_pool_;
}

const MySqlClientPool& MySqlReadWriteClientPool::Writer() const {
    return *writer_pool_;
}

MySqlClientPool& MySqlReadWriteClientPool::ReaderOrWriter() {
    return reader_pool_ != nullptr ? *reader_pool_ : *writer_pool_;
}

const MySqlClientPool& MySqlReadWriteClientPool::ReaderOrWriter() const {
    return reader_pool_ != nullptr ? *reader_pool_ : *writer_pool_;
}

}  // namespace common::mysql
