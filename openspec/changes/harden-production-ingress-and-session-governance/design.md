# Design

## Production Ingress Boundary

生产接入采用 Edge/LB 终止 HTTPS/TLS。Nginx、Envoy 或云 LB 负责公网 TLS、证书生命周期、反向代理、基础限流和公网端口暴露。`api_gateway_server` 不内建 TLS，继续作为内网 HTTP upstream 运行。

生产部署要求：

- 公网 HTTPS 使用 TLS 1.2/1.3。
- 证书和私钥不进入仓库。
- Edge/LB 到 `api_gateway_server` 走内网。
- 后端业务服务不从公网暴露。
- `auth_server`、`world_server`、`scene_server`、`instance_server`、`player_server`、`social_server` 继续只接受 signed internal RPC。

`game_gateway_server` 是游戏可靠长连接入口。公网是否使用 TCP TLS、WSS 或 L4 TLS 由后续实现 change 单独决定；本 change 只要求它与 API Gateway 一样处在明确的 Edge/LB 暴露边界内。

## Token And Ticket Governance

`access_token` 用于 API 访问身份，`gateway_ticket` 用于进入 Game Gateway。两者都需要从当前 HMAC token 雏形升级为生产可治理格式：

```text
version
key_id
issuer
audience
purpose
account_id
player_id
session_token or session_id
issued_at_epoch_millis
expires_at_epoch_millis
nonce or jti
signature
```

`access_token` 的 audience 默认面向 API Gateway。`gateway_ticket` 的 audience 默认面向 Game Gateway，并保持短 TTL。签名 key 支持 active key 和 previous key：签发只使用 active key，验签允许 previous key 在配置窗口内继续通过，用于平滑轮换。

`gateway_ticket` 必须设计 replay guard。MVP 可先使用本地 consumed-ticket cache；生产路径预留 Redis replay cache，以 `jti` 或 ticket digest 为 key，TTL 与 ticket 过期时间一致。重复消费必须拒绝。

安全日志不得输出 token、ticket 或完整签名。错误响应不暴露签名失败、过期、key mismatch、replay 的内部细节，只返回稳定错误码和通用错误信息。

## Game Session Governance

`GateLogin` 通过 ticket 校验后，Game Gateway 创建 `game_session_id`，并绑定：

```text
game_session_id
connection_id
gateway_id
account_id
player_id
session_token or auth_session_id
issued_at_epoch_millis
last_seen_epoch_millis
expires_at_epoch_millis
client_device_id
```

Game Gateway 对所有 session-bound public request 校验 `game_session`，而不是只依赖 `player_id/session_token`。本地内存 registry 作为 MVP 实现边界保留，接口设计预留 Redis online/session registry。

会话治理策略：

- heartbeat 定期更新 `last_seen`。
- heartbeat timeout 后 session 失效，后续 session-bound request 返回 401。
- 同一 `player_id` 重复 GateLogin 默认踢下旧连接并替换 session。
- reconnect ticket 只能短 TTL 使用，且绑定 `game_session_id/player_id/gateway_id`。
- unbind 必须清理本地 session、reconnect ticket 和 replay cache 中相关短期状态。

## Observability And Audit

新增或扩展指标：

- `api_login_success_total`
- `api_login_failed_total`
- `gateway_ticket_issued_total`
- `gateway_ticket_rejected_total`
- `gateway_ticket_replay_total`
- `gate_login_success_total`
- `gate_login_failed_total`
- `game_session_expired_total`
- `reconnect_success_total`
- `reconnect_failed_total`
- `internal_auth_failed_total`

日志字段至少包含 `request_id`、`trace_id`、`player_id`、`gateway_id`、`game_session_id`、`error_code` 和事件名。日志不记录 token/ticket 原文。

## KCP And Realtime Posture

KCP/UDP 在本阶段暂缓。`GET /v1/servers` 可以继续保留 realtime endpoint 字段，但该字段在 OpenSpec 中视为 reserved/disabled until realtime phase。

本 change 不新增 `realtime_gateway_server` runtime，不新增 RealtimeBind 协议，不接移动、AOI、战斗或场景实时同步。后续 realtime phase 必须复用本 change 定义的 ticket、session、日志和暴露边界。

## Implementation Order

后续实现按以下顺序推进：

1. Edge TLS sample/config：补 Nginx/Envoy 示例、HTTPS upstream 约束和生产部署说明。
2. Token key rotation：扩展 token claims、配置 active/previous key、issuer/audience。
3. Ticket replay guard：实现 consumed ticket cache，并预留 Redis replay cache。
4. Game session governance：新增 `game_session_id`、heartbeat timeout、重复登录策略和 reconnect ticket。
5. Metrics/logging：补安全指标、审计日志字段和敏感字段脱敏。
6. Verification：补过期、篡改、重复消费、未绑定 session、heartbeat timeout 的负向验证。
