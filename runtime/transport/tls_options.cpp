// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/transport/tls_options.h"

namespace framework::transport {

TlsOptions ReadTlsOptions(const common::config::SimpleConfig& config, const std::string& prefix) {
    TlsOptions options;
    options.enabled = config.GetBool(prefix + "enabled", options.enabled);
    options.cert_file = config.GetString(prefix + "cert_file", options.cert_file);
    options.key_file = config.GetString(prefix + "key_file", options.key_file);
    options.ca_file = config.GetString(prefix + "ca_file", options.ca_file);
    options.server_name = config.GetString(prefix + "server_name", options.server_name);
    options.verify_peer = config.GetBool(prefix + "verify_peer", options.verify_peer);
    return options;
}

}  // namespace framework::transport
