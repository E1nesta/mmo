# Phase 1: Server Foundation And Auth Data Layer

## Summary

本 change 将登录身份从 demo hash/mock 迁移到 MySQL-backed 数据层，并补齐最小服务器底座：存储依赖初始化、API readiness、schema seed、登录负向验证。

## Motivation

当前接入层、Gateway、internal RPC、token/ticket/session 已经成型，但 `AuthService` 仍通过 account/device hash 派生账号和玩家身份。进入 MMO Scene/Entity/Tick 框架前，需要真实 `account_id/player_id` 数据来源，否则后续玩家实体、角色数据和持久化都会建立在 demo 身份之上。

## Goals

- `auth_server` 启动时初始化 MySQL pool，失败即启动失败。
- 登录通过 `AccountRepository` 和 `PlayerIdentityRepository` 查询 MySQL。
- 密码校验使用 PBKDF2-SHA256，不保留明文或 mock fallback。
- 本地 MySQL schema 自动包含 smoke 账号和玩家身份。
- API Gateway 增加 `/ready`，至少检查 Auth backend 可达。
- 保持现有 local flow 通过，并新增登录负向验证。

## Non-Goals

- 不做注册、找回密码、多角色创建。
- 不做背包、任务、装备等业务系统。
- 不做 Redis 分布式 session。
- 不接 KCP/Realtime Gateway。
