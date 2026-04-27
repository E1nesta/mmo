// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/foundation/id/id_generator.h"

#include <chrono>

namespace common::id {

namespace {

constexpr std::int64_t kCustomEpochMs = 1704067200000LL;

}  // namespace

IdGenerator::IdGenerator(std::uint16_t node_id) : node_id_(node_id) {}

std::int64_t IdGenerator::Next() {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count() -
                        kCustomEpochMs;
    const auto seq = sequence_.fetch_add(1, std::memory_order_relaxed) & 0x0FFFu;
    return (now_ms << 22) | (static_cast<std::int64_t>(node_id_ & 0x03FFu) << 12) | static_cast<std::int64_t>(seq);
}

std::string IdGenerator::NextString() {
    return std::to_string(Next());
}

}  // namespace common::id
