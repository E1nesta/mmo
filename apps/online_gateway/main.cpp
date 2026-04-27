// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/online_gateway/online_gateway_server_app.h"

int main(int argc, char* argv[]) {
    services::online_gateway::OnlineGatewayServerApp app;
    return app.Main(argc, argv);
}
