# Self-developed MMO Runtime With Production Infrastructure

## Summary

本 change 固化项目长期架构原则：游戏主链路继续自研 MMO Runtime，外围生产能力接入主流基础设施。该原则用于约束后续选型，避免把 MMO 热路径误替换成通用 HTTP/RPC/服务治理框架，也避免自研 TLS、HA、观测、编排等成熟基础设施能力。

## Motivation

本项目目标是高性能高并发 MMO 后端框架，而不是普通 CRUD 微服务。MMO 主链路需要长连接、连接态、Gateway 路由、player_id 分片、Scene 内存权威、AOI、实时同步、过载保护和断线重连。这些能力需要在项目 runtime 中清晰建模。

同时，TLS 终止、负载均衡、MySQL/Redis 高可用、schema migration、指标采集、日志聚合、容器编排等生产基础设施已经有成熟主流方案，不应在本项目中重复自研。

## Goals

- 明确 `runtime/*` 和 `modules/scene|aoi|combat` 等游戏热路径能力继续自研。
- 明确 Edge/LB、MySQL HA、Redis HA、Prometheus/OpenTelemetry、Kubernetes、schema migration 等外围能力采用主流基础设施。
- 明确 gRPC/brpc/service mesh 等不能在没有专门 OpenSpec 和收益论证的情况下替换游戏主链路。
- 为后续阶段排序提供边界：优先 metrics exporter、migration、readiness、reconnect/session 接管、压测。

## Non-Goals

- 本 change 不新增代码。
- 不引入 Kubernetes、Envoy、Prometheus、OpenTelemetry、Flyway/Liquibase 的具体实现。
- 不重命名 `runtime/channel`、`runtime/rpc`。
- 不改变现有 TCP Envelope、Protobuf schema 或 internal auth 机制。
