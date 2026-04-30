# Storage Governance And Redis Session Runtime

## Summary

本 change 将 Phase 1 的 MySQL/Redis 基础设施继续推进到生产级 MMO 数据底座：`player_server` 使用 MySQL-backed repository 和事务化奖励幂等写入，`game_gateway_server` 使用 Redis-backed gateway ticket replay guard、game session store 和 online registry。目标是把长期资产、短期会话、在线态的权威边界定型。

## Motivation

当前 Auth 已经迁移到 MySQL-backed 登录，但 Player 资产仍停留在内存服务，Game Gateway 的 ticket replay 和 session/online 状态也仍是本地内存。对于生产级 MMO 骨架，这会导致重启丢资产、重复奖励入账风险、多网关会话不可治理和 ticket replay 无法跨进程防护。

## Goals

- `player_server` 启动时初始化 MySQL pool，失败即启动失败。
- Player 长期资产写入通过 MySQL transaction + `reward_ledger` 幂等流水完成。
- Game Gateway 启动时初始化 Redis pool，失败即启动失败。
- Gateway ticket replay guard 使用 Redis `SET NX` + TTL，跨进程一次性消费。
- GateLogin 成功后把 `game_session` 和 `online:{player_id}` 写入 Redis，并随 heartbeat 刷新 TTL。
- 增加轻量 probe 覆盖奖励幂等、Redis ticket replay、Redis session/online。

## Non-Goals

- 不做分库分表、读写分离、MySQL Router/ProxySQL 部署代码。
- 不做 Redis Cluster/Sentinel 部署代码。
- 不引入 ORM。
- 不做背包、装备、货币多表模型。
- 不做跨服、跨区、KCP 实时主链路。
