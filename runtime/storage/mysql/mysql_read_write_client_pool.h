// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/storage/mysql/mysql_client.h"
#include "runtime/storage/mysql/mysql_client_pool.h"

#include <memory>

namespace common::mysql {

class MySqlReadWriteClientPool {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit MySqlReadWriteClientPool(ReadWritePoolOptions options);

    bool Initialize(std::string* error_message = nullptr);

    [[nodiscard]] bool HasReader() const;
    [[nodiscard]] MySqlClientPool& Writer();
    [[nodiscard]] const MySqlClientPool& Writer() const;
    [[nodiscard]] MySqlClientPool& ReaderOrWriter();
    [[nodiscard]] const MySqlClientPool& ReaderOrWriter() const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    std::unique_ptr<MySqlClientPool> writer_pool_;
    std::unique_ptr<MySqlClientPool> reader_pool_;
};

}  // namespace common::mysql
