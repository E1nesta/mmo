# Design

## Architecture Principle

项目采用“双层框架”原则：

- 主框架：自研 MMO Runtime，负责游戏热路径和领域特有运行时。
- 外围框架：主流生产基础设施，负责部署、治理、观测、存储高可用和安全入口。

这不是“全部自研”，也不是“全部交给通用微服务框架”。边界由流量性质和职责决定。

## Self-developed Runtime Boundary

以下能力属于本项目核心差异，应继续在仓库内自研和演进：

- `runtime/transport`：TCP Envelope、后续 KCP/UDP realtime transport。
- `runtime/rpc`、`runtime/channel`、`runtime/routing`：游戏服务间通信和 Gateway 转发。
- `runtime/session`：gateway ticket、game session、online registry、reconnect。
- `runtime/execution`：IOContextPool、ShardedExecutor、SceneWorker、player/scene shard。
- `modules/scene`、`modules/aoi`、`modules/combat`：Scene 内存权威、AOI、移动/战斗热路径。
- Gateway 接入与 session-bound request 校验。
- MMO 过载保护、handler queue、pending limit 和热路径 metrics。

## Production Infrastructure Boundary

以下能力默认采用成熟主流基础设施，仓库只提供配置、适配、文档和必要运行时集成：

- Edge/LB：Nginx、Envoy、Cloud LB，用于 TLS 终止、L4/L7 转发、基础限流。
- 数据高可用：MySQL InnoDB Cluster/MySQL Router 或等价方案，Redis Sentinel/Cluster。
- Schema migration：Flyway/Liquibase 或等价工具。
- Observability：Prometheus、OpenTelemetry、Grafana/Loki 等。
- Deployment：Docker Compose 用于 local，Kubernetes 用于后续生产部署。
- Secrets/config：环境变量、Secret/ConfigMap 或云密钥系统。

## Framework Selection Guardrails

gRPC、brpc、service mesh、NATS/Kafka、完整 Actor 框架等可以作为后续局部能力被评估，但不得默认替换游戏主链路。若要引入，必须另开 OpenSpec，说明：

- 替换的是哪条链路。
- 对长连接、session、player shard、Scene ownership 的影响。
- 性能、复杂度、运维和学习成本。
- 回滚路径。

## Recommended Next Implementation Order

在该原则下，后续阶段优先顺序为：

1. Prometheus metrics exporter 和核心 runtime metrics。
2. MySQL schema migration/version 管理。
3. 完整 readiness：MySQL、Redis、关键 backend internal health。
4. Game Gateway duplicate login、reconnect ticket、Redis session 接管。
5. TCP 压测 client 和基础故障注入。
