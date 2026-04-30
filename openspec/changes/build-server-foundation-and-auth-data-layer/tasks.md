## 1. OpenSpec

- [x] 1.1 新增 proposal/design/tasks/spec artifacts。

## 2. Server Foundation

- [x] 2.1 新增 storage bootstrap helper。
- [x] 2.2 `auth_server` 启动时初始化 MySQL pool，失败即退出。
- [x] 2.3 API Gateway 新增 `/ready`。
- [x] 2.4 本阶段不新增 TCP 后端 HTTP 管理端口。

## 3. Auth Data Layer

- [x] 3.1 新增 `AccountRepository` 和 `PlayerIdentityRepository`。
- [x] 3.2 新增 MySQL repository 实现。
- [x] 3.3 新增 PBKDF2-SHA256 `PasswordHasher`。
- [x] 3.4 `AuthService` 改为 repository-backed login。
- [x] 3.5 登录失败统一返回 401，不泄露内部原因。

## 4. Schema And Local Flow

- [x] 4.1 扩展 MySQL schema 和 local seed。
- [x] 4.2 统一 `player_profiles` 表名。
- [x] 4.3 `scripts/dev_up.sh` 等待依赖 ready 并重放 schema。
- [x] 4.4 `scripts/run_local_flow.sh` 启动依赖并运行 auth data probe。

## 5. Verification

- [x] 5.1 `git diff --check`
- [x] 5.2 CMake configure/build
- [x] 5.3 `scripts/dev_up.sh`
- [x] 5.4 `auth_data_probe`
- [x] 5.5 `internal_auth_probe`、`security_governance_probe`
- [x] 5.6 `scripts/run_local_flow.sh`
