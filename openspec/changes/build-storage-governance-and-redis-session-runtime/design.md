# Design

## Data Ownership

MySQL 是长期资产和身份数据的 source of truth。Redis 只承载短期 ticket、session、online 状态和缓存类数据，Redis 数据丢失不能造成玩家长期资产丢失。

Player Service 是玩家长期资产写入边界。Instance/Scene/Gateway 只能通过 Player Service 请求奖励应用，不能直接写 `player_profiles` 或后续资产表。

## Player MySQL Transaction

`MysqlPlayerRepository` 改为基于 `MysqlConnectionPool` 获取连接。新增 repository 级 `apply_reward_once()`，在单个事务内完成：

1. 确保 `player_profiles` 行存在。
2. 插入 `reward_ledger`，唯一键命中表示重复请求。
3. 对 `player_profiles` 做增量更新。
4. 读取更新后的 profile。
5. commit；任何异常 rollback。

`PlayerService` 在存在 repository 时走 repository 事务；没有 repository 时保留本地内存路径用于轻量场景。

## Redis Ticket And Session Runtime

新增 Redis-backed `TicketReplayStore`：

- key: `ticket_replay:{jti}`
- command: `SET key value NX PX ttl`
- 第二次消费同一 jti 必须失败。

新增 Redis-backed session store：

- `game_session:{game_session_id}` 存储 player/session/gateway/expiry/last_seen 等连接态。
- `online:{player_id}` 指向当前 `game_session_id` 和 gateway。
- GateLogin 成功后写入 Redis。
- Ping/heartbeat 成功后刷新 `last_seen` 和 TTL。
- session-bound 请求同时验证本地 binding 和 Redis binding。

## Startup And Readiness

`player_server` 初始化 MySQL pool，失败即退出。`game_gateway_server` 初始化 Redis pool，失败即退出。API Gateway 的 `/ready` 暂时仍以 Auth backend 可达为最小 readiness；更完整的 multi-backend readiness 后续 change 处理。

## Validation

新增 `storage_governance_probe`：

- MySQL reward apply 第一次成功。
- 同一 idempotency key 第二次不重复入账。
- Redis ticket replay 第一次成功、第二次失败。
- Redis session store 能保存、验证、touch game session 和 online 状态。
