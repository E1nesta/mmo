# harden-production-ingress-and-session-governance

## Why

当前项目已经完成 `api_gateway_server`、`game_gateway_server`、`access_token`、`gateway_ticket` 和 signed internal RPC 的第一阶段闭环，但它仍然偏框架 MVP：API Gateway 是内网 HTTP 服务，token/ticket 尚未设计 key rotation 和 replay guard，Game Gateway session 仍是本地连接绑定模型。

生产级商业 MMO 后端需要先把公网接入安全、token/ticket 治理、Game Gateway 会话治理、后端暴露面和观测验收边界定清楚，再继续扩展 KCP/UDP 或玩法业务。本 change 只固化这些边界和后续实现任务，不实现 realtime runtime。

## What

- 采用 Edge/LB 终止 HTTPS/TLS 的生产接入模型，`api_gateway_server` 保持内网 HTTP。
- 明确 public 只暴露 Edge/LB、`api_gateway_server` 和 `game_gateway_server`，后端服务继续 internal-only。
- 规划 token/ticket 字段增强、key id、issuer、audience、issued_at、expires_at、nonce/jti 和 secret rotation。
- 规划 `gateway_ticket` 短 TTL、一次性消费和 Redis replay cache 接入边界。
- 规划 Game Gateway 生成 `game_session_id`，并治理 heartbeat、reconnect、重复登录、session 过期和 unbind。
- 明确 KCP/UDP 本阶段暂缓，不新增 `realtime_gateway_server` runtime，不接移动、AOI 或战斗实时链路。
- 规划登录、GateLogin、ticket、session、internal auth 的指标和安全日志要求。

## Impact

- 本 change 只新增 OpenSpec 文档，不修改 runtime、proto、CMake、配置或脚本。
- 后续实现将影响 API Gateway、Auth token/ticket、Game Gateway session、metrics/logging、部署样例和验证工具。
- 生产 HTTPS/TLS 由 Nginx、Envoy 或云 LB 承担；证书私钥不进入仓库。

## References

- [NGINX SSL Termination](https://docs.nginx.com/nginx/admin-guide/security-controls/terminating-ssl-http/)
- [OWASP Session Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)
- [RFC 9700 OAuth 2.0 Security Best Current Practice](https://datatracker.ietf.org/doc/html/rfc9700)
