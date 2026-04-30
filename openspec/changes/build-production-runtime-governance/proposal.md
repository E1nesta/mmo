# Production Runtime Governance

## Summary

本 change 将 Phase 2 定义为生产级运行治理：在现有自研 MMO Runtime、Gateway、MySQL-backed Auth、Redis-backed Session 的基础上，补齐可观测、readiness、schema migration、结构化日志和本地验证 probe。目标是让服务不仅能跑通，还能被运维系统发现、判断健康、定位问题，并为后续 Gateway 会话治理、压测和玩法扩展提供稳定底座。

## Motivation

Phase 1 已经把登录身份、Gateway session、Redis ticket replay、Player 奖励幂等写入接入到真实数据层。但当前运行治理仍偏弱：指标只停留在内存 snapshot，没有标准 scrape 输出；readiness 只覆盖 API Gateway 的 Auth backend；MySQL schema 仍依赖 init SQL 重放；日志字段没有形成跨服务强约束；local flow 对治理能力的负向验证不足。

对于生产级 MMO 框架，运行治理是进入玩法开发前必须补齐的底座。否则后续并发压测、重连治理、数据扩展和故障排查都会缺少可靠信号。

## Goals

- 新增 Prometheus text format metrics exporter 能力，至少覆盖已有 `MetricsRegistry` 字段。
- API Gateway 暴露 `/metrics`，Game Gateway 和核心 TCP 后端具备可接入 metrics exporter 的统一运行时组件。
- 扩展 readiness governance：区分 liveness 与 readiness，ready 必须检查关键依赖和下游 backend。
- 新增 schema migration/versioning 机制，不再把生产 schema 演进依赖 `init.sql` 重放。
- 统一结构化日志字段，形成 request、player、service、upstream、error_code 的最小日志契约。
- 新增 production runtime governance probe，验证 metrics/readiness/migration/logging 的关键路径。
- 保持 `scripts/run_local_flow.sh` 一键 smoke 能力，并加入新 probe。

## Non-Goals

- 不实现背包、任务、装备、商城、战斗、AOI 等玩法系统。
- 不接 KCP/UDP，不新增 `realtime_gateway_server` runtime。
- 不部署 Prometheus、Grafana、Loki、OpenTelemetry Collector 或 Kubernetes。
- 不引入服务网格、注册中心、mTLS、SPIFFE/Istio。
- 不把主游戏链路替换为 gRPC/brpc。
- 不做 MySQL HA、读写分离、Redis Sentinel/Cluster 部署代码。
- 不改现有 Envelope wire format 或 public/internal protobuf 业务语义。

## Impact

- `runtime/observability` 增加 metrics exposition 和日志字段约束。
- `runtime/storage` 增加 migration/versioning helper 或 runner。
- `apps/api_gateway_server` 增加 `/metrics`，并扩展 `/ready` 的依赖检查。
- `apps/game_gateway_server` 和核心后端服务接入统一 readiness/metrics 组件的最小能力。
- `scripts/dev_up.sh`、`scripts/run_local_flow.sh` 加入 migration/probe 验证。
- `deploy/mysql` 从单一 init schema 逐步迁移到 versioned migrations。

## Risks

- Readiness 检查如果过重，可能在高并发下影响服务响应。
- Metrics exporter 如果直接复用业务 IO 线程并做阻塞操作，可能影响主链路。
- Migration runner 如果不可重入，会破坏本地重复执行的开发体验。
- 结构化日志如果误输出 token/ticket/password，会引入安全风险。

## Mitigation

- Readiness 只做轻量依赖检查，并设置短超时。
- Metrics exporter 只输出内存 snapshot，不做 DB/Redis 查询。
- Migration runner 使用 `schema_migrations` 表记录版本，已应用版本必须跳过。
- 日志契约明确禁止输出 password、access_token、gateway_ticket、session_token 原文。
- 新增 probe 覆盖重复 migration、metrics 内容、依赖不可达 ready 失败和敏感字段检查。

## Verification

- `git diff --check`
- `cmake -S /home/love/code/mmo -B /home/love/code/mmo/build/production-runtime-governance -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build /home/love/code/mmo/build/production-runtime-governance --parallel`
- `scripts/dev_up.sh`
- 新增并运行 `production_runtime_governance_probe`
- 保留并运行 `auth_data_probe`、`storage_governance_probe`、`security_governance_probe`、`internal_auth_probe`
- `scripts/run_local_flow.sh`
