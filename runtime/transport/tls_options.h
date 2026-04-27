// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"

#include <string>

namespace framework::transport {

struct TlsOptions {
    bool enabled = false;
    std::string cert_file;
    std::string key_file;
    std::string ca_file;
    std::string server_name;
    bool verify_peer = false;
};

[[nodiscard]] TlsOptions ReadTlsOptions(const common::config::SimpleConfig& config,
                                        const std::string& prefix);

}  // namespace framework::transport
