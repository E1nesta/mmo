// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"

#include <string>

namespace common::config {

bool ValidateAuthStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidateApiGatewayStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidatePlayerQueryStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidatePlayerWriteStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidateDungeonRuntimeStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidateOnlineGatewayStorageConfig(const SimpleConfig& config, std::string* error_message);
bool ValidateSocialStorageConfig(const SimpleConfig& config, std::string* error_message);

}  // namespace common::config
