# Design

## Runtime Governance Boundary

本 change 补齐生产运行治理，不改变 MMO 主链路架构。自研 Runtime 继续承载 TCP/Envelope/RPC/Session/Execution；外围生产基础设施如 Prometheus、Grafana、Nginx、Kubernetes 后续可接入，但本仓库本轮只提供标准化暴露点和本地验证能力。

治理面分为五类：

- metrics：服务运行指标，面向 scrape。
- readiness：服务是否可以接流量，面向部署和本地 flow。
- migration：schema 是否处于预期版本，面向数据演进。
- structured logging：故障定位字段，面向排障和审计。
- probes：本地可重复验证，面向开发和 CI 前置检查。

## Metrics Exporter

新增 Prometheus text format exporter，输入 `MetricsSnapshot`，输出稳定的 counter/gauge 文本。已有字段优先直接映射：

- `active_connections` -> gauge
- `accepted_connections_total` -> counter
- `requests_total` -> counter
- `errors_total` -> counter
- `bytes_read_total` / `bytes_written_total` -> counter
- `handler_latency_ms_total` -> counter
- `executor_queue_depth` -> gauge
- `executor_queue_overflow_total` -> counter
- `handler_rejected_total` -> counter
- `channel_pending_count` / `rpc_pending_count` -> gauge
- `channel_pending_limit_total` -> counter
- `rpc_timeout_total` / `rpc_remote_error_total` -> counter
- `login_success_total` / `login_failed_total` -> counter
- `gateway_ticket_*`、`gate_login_*`、`game_session_expired_total`、`internal_auth_failed_total` -> counter

指标命名统一使用 `mmo_` 前缀。Exporter 只读取内存 snapshot，不主动访问 MySQL、Redis 或下游服务。

API Gateway 本轮直接暴露 `GET /metrics`。TCP 服务不新增独立公网 HTTP 管理端口；如需暴露 metrics，可通过一个轻量内网 management listener 或后续统一 runtime admin server 接入。本 change 先固化组件边界和 API Gateway 落点，避免把治理 HTTP 逻辑散落到每个业务 main。

## Readiness Governance

`/health` 表示进程存活，不检查外部依赖。`/ready` 表示服务是否可以接业务流量，必须检查关键依赖。

最小 ready 规则：

- API Gateway：Auth backend 可达；如果 server list 依赖 Game Gateway 地址，也应校验配置存在。
- Game Gateway：Redis 可达；已配置的后端 route target 至少可以解析到服务配置。
- Auth Server：MySQL 可达。
- Player Server：MySQL 可达。
- 其他 TCP 后端：内部 RPC 配置有效，必要依赖可达。

Readiness 检查必须短超时、无副作用、可重复执行。依赖失败返回 503 和稳定错误码，响应不得泄露密码、token、ticket、shared secret 或内部签名细节。

## Schema Migration

新增 versioned migration 机制，生产 schema 演进以 migration 为准，`deploy/mysql/init/001_schema.sql` 保留为 local bootstrap/重置用。

设计要求：

- 新增 `schema_migrations` 表，记录 `version`、`name`、`checksum`、`applied_at`。
- migration 文件按版本排序执行，例如 `deploy/mysql/migrations/0001_init.sql`。
- 已应用且 checksum 一致的 migration 必须跳过。
- 已应用但 checksum 不一致必须失败，禁止静默覆盖。
- migration runner 可以被 `scripts/dev_up.sh` 或独立 probe 调用。
- 本轮不引入 Flyway/Liquibase runtime 依赖；如果后续接入外部 migration 工具，必须兼容 `schema_migrations` 或明确迁移策略。

Local 开发仍允许重放 init SQL 来重置数据，但运行时和生产设计应以 versioned migrations 为准。

## Structured Logging

统一日志上下文字段最小契约：

```text
event
service
request_id
trace_id
player_id
account_id
gateway_id
game_session_id
upstream
error_code
status
latency_ms
```

字段没有值时可以省略或输出空值，但 key 名称必须稳定。日志输出继续使用现有 `runtime/observability/logging`，本 change 扩展 `LogContext` 或增加 helper，以避免各服务手写不一致格式。

敏感字段禁止进入日志：

- password
- password_hash
- access_token
- gateway_ticket
- session_token
- internal_signature
- shared_secret

错误日志应记录内部原因对应的稳定 `error_code`，但客户端响应继续保持泛化错误信息。

## Probes And Flow

新增 `production_runtime_governance_probe`，覆盖：

- Metrics exporter 能输出 Prometheus text format。
- 关键 metrics 名称存在，counter/gauge 类型符合预期。
- migration runner 第一次执行成功，第二次执行跳过已应用版本。
- migration checksum mismatch 失败。
- readiness helper 在依赖可达时 ready，在依赖不可达时 not ready。
- structured logging helper 不输出 token/ticket/password/session_token 原文。

`scripts/run_local_flow.sh` 在已有 `auth_data_probe`、`storage_governance_probe` 后运行该 probe，确保治理面不会破坏现有 smoke flow。

## Implementation Order

1. 新增 metrics text exporter，并为 `MetricsSnapshot` 建立稳定指标映射。
2. API Gateway 接入 `GET /metrics`。
3. 新增 readiness helper，先收敛 API Gateway 现有 `/ready` 逻辑，再为后续 TCP 服务复用。
4. 新增 schema migration runner 和 `schema_migrations` 表。
5. 扩展 structured logging context 和脱敏约束。
6. 新增 `production_runtime_governance_probe`。
7. 更新 `scripts/dev_up.sh`、`scripts/run_local_flow.sh`。
8. 编译、probe、flow 验证。
