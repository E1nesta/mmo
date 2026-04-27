// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <string>

namespace framework::runtime {

struct ServiceCliOptions {
    std::string service_name;
    std::string config_path;
    bool check_only = false;
    bool show_version = false;
};

ServiceCliOptions ParseServiceOptions(int argc,
                                      char* argv[],
                                      std::string default_service_name,
                                      std::string default_config_path);

}  // namespace framework::runtime
