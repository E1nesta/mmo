// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/transport/service_options.h"

namespace framework::runtime {

ServiceCliOptions ParseServiceOptions(int argc,
                                      char* argv[],
                                      std::string default_service_name,
                                      std::string default_config_path) {
    ServiceCliOptions options;
    options.service_name = std::move(default_service_name);
    options.config_path = std::move(default_config_path);

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--config" && index + 1 < argc) {
            options.config_path = argv[++index];
        } else if (arg == "--check") {
            options.check_only = true;
        } else if (arg == "--version") {
            options.show_version = true;
        }
    }

    return options;
}

}  // namespace framework::runtime
