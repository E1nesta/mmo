# Design

## Server Foundation

新增 storage bootstrap helper，将 `foundation::ServerConfig` 转换为 `runtime/storage` 使用的 MySQL/Redis config，并统一初始化连接池。`auth_server` 本阶段必须初始化 MySQL pool，初始化失败直接返回非 0，避免回退到 mock 登录。

API Gateway 保留 `/health` 作为进程存活检查，新增 `/ready` 作为依赖就绪检查。Phase 1 的 readiness 只要求 Auth backend TCP 可达；后续可扩展到内部 health RPC 或管理端口。

## Auth Data Layer

Auth 模块新增两个 repository 边界：

- `AccountRepository`：按账号名读取账号、密码 hash、盐、迭代次数和状态。
- `PlayerIdentityRepository`：按 account_id 读取 primary player_id。

`AuthService` 不再派生 demo identity。登录流程为：

1. 查询 account。
2. 检查账号状态。
3. 使用 PBKDF2-SHA256 校验密码。
4. 查询 primary player identity。
5. 生成 session_token 并返回 account/player identity。

客户端失败响应统一为 `401 login authentication failed`。内部日志可记录 `account is banned`、`password is invalid`、`player identity is missing` 等原因，但不得输出密码、token、ticket。

## Schema And Seed

`deploy/mysql/init/001_schema.sql` 新增：

- `accounts`
- `player_identities`

并保留 `player_profiles`、`reward_ledger`。本阶段同时修复 player repository 对 `player_profile` 单数表的引用，统一为 `player_profiles`。

本地 seed 固定：

- `demo_player / demo_password` -> `account_id=1098216`, `player_id=1198216`
- `demo_banned / demo_password` -> banned account
- `demo_missing_identity / demo_password` -> no player identity

## Local Flow

`scripts/run_local_flow.sh` 在启动服务前拉起 docker compose MySQL/Redis，并在服务启动后运行：

- `internal_auth_probe`
- `auth_data_probe`
- `mmo_flow_client`

`auth_data_probe` 直接验证 MySQL-backed AuthService 行为，`mmo_flow_client` 保持完整 Gateway flow。
