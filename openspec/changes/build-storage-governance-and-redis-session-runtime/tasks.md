## 1. OpenSpec

- [x] 1.1 新增 proposal/design/tasks/spec artifacts。

## 2. Player MySQL Governance

- [x] 2.1 扩展 MySQL client 支持 transaction 和 duplicate-key 判断。
- [x] 2.2 扩展 `PlayerRepository` 契约，新增事务化奖励幂等写入。
- [x] 2.3 改造 `MysqlPlayerRepository` 使用 MySQL pool 和 transaction。
- [x] 2.4 `player_server` 启动时初始化 MySQL pool 并注入 repository。

## 3. Redis Session Runtime

- [x] 3.1 扩展 Redis client 支持 `SET NX PX` 和 `SET PX`。
- [x] 3.2 新增 Redis-backed ticket replay store。
- [x] 3.3 新增 Redis-backed game session/online store。
- [x] 3.4 `game_gateway_server` 启动时初始化 Redis pool。
- [x] 3.5 GateLogin/Ping/session-bound validation 接入 Redis runtime。

## 4. Probes And Flow

- [x] 4.1 新增 `storage_governance_probe`。
- [x] 4.2 `scripts/run_local_flow.sh` 加入 storage governance probe。

## 5. Verification

- [x] 5.1 `git diff --check`
- [x] 5.2 CMake configure/build
- [x] 5.3 `scripts/dev_up.sh`
- [x] 5.4 `storage_governance_probe`
- [x] 5.5 `auth_data_probe`、`internal_auth_probe`、`security_governance_probe`
- [x] 5.6 `scripts/run_local_flow.sh`
