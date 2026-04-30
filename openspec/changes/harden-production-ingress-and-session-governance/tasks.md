## 1. Edge TLS And Deployment Boundary

- [x] 1.1 新增 Edge/LB HTTPS termination 示例配置或部署文档。
- [x] 1.2 明确 `api_gateway_server` 只作为内网 HTTP upstream，不内建 TLS。
- [x] 1.3 明确证书私钥不进入仓库，生产使用 TLS 1.2/1.3。
- [x] 1.4 明确公网只暴露 Edge/LB、API Gateway 和 Game Gateway，后端服务 internal-only。

## 2. Token And Ticket Governance

- [x] 2.1 扩展 token claims：`version`、`key_id`、`issuer`、`audience`、`issued_at`、`expires_at`、`nonce/jti`。
- [x] 2.2 新增 active key + previous key 验签窗口配置。
- [x] 2.3 将 `access_token` audience 限定到 API Gateway。
- [x] 2.4 将 `gateway_ticket` audience 限定到 Game Gateway，并保持短 TTL。
- [x] 2.5 保证 token/ticket 不写入日志，错误响应不泄露签名细节。

## 3. Gateway Ticket Replay Guard

- [x] 3.1 实现 gateway ticket 一次性消费语义。
- [x] 3.2 MVP 使用本地 consumed-ticket cache。
- [x] 3.3 预留 Redis replay cache 接口，key 使用 `jti` 或 ticket digest，TTL 对齐 ticket 过期时间。
- [x] 3.4 重复消费 ticket 返回 401，并记录 replay 指标。

## 4. Game Session Governance

- [x] 4.1 `GateLogin` 成功后生成 `game_session_id`。
- [x] 4.2 将 session 绑定扩展为 `game_session_id/connection_id/gateway_id/player_id/session_token/last_seen/expires_at`。
- [x] 4.3 session-bound request 改为校验 `game_session`。
- [x] 4.4 实现 heartbeat timeout 和 session expiry。
- [x] 4.5 实现重复登录策略：同一 `player_id` 新 session 替换旧 session。
- [x] 4.6 完善 reconnect ticket：短 TTL，绑定 `game_session_id/player_id/gateway_id`。

## 5. Observability And Audit

- [x] 5.1 增加 API 登录成功/失败指标。
- [x] 5.2 增加 ticket issued/rejected/replay 指标。
- [x] 5.3 增加 GateLogin 成功/失败、session expired、reconnect 成功/失败指标。
- [x] 5.4 增加 internal auth failed 指标。
- [x] 5.5 日志补充 `gateway_id`、`game_session_id`、`error_code`，并确认不输出 token/ticket 原文。

## 6. KCP Postponed Boundary

- [x] 6.1 不新增 `realtime_gateway_server` runtime。
- [x] 6.2 不新增 RealtimeBind 协议。
- [x] 6.3 不接移动、AOI、战斗或场景实时同步。
- [x] 6.4 将 `GET /v1/servers` 的 realtime endpoint 标记为 reserved/disabled until realtime phase。

## 7. Verification

- [x] 7.1 运行 `git diff --check`。
- [x] 7.2 运行 CMake configure/build。
- [x] 7.3 运行 `scripts/run_local_flow.sh`。
- [x] 7.4 新增并运行负向验证：过期 ticket、篡改 ticket、重复消费 ticket、未绑定 session 请求、heartbeat timeout 后请求被拒绝。
