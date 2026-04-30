## 1. OpenSpec

- [x] 1.1 新增 proposal/design/tasks/spec artifacts。
- [x] 1.2 明确本 change 只做生产运行治理，不做玩法、不接 KCP、不部署外部观测平台。

## 2. Metrics Exporter

- [x] 2.1 新增 Prometheus text format exporter，将 `MetricsSnapshot` 映射为 `mmo_` 前缀指标。
- [x] 2.2 明确 counter/gauge 类型，并覆盖已有 runtime、RPC、Gateway、安全指标字段。
- [x] 2.3 API Gateway 新增 `GET /metrics`。
- [x] 2.4 确保 exporter 只读内存 snapshot，不访问 MySQL/Redis/下游 backend。

## 3. Readiness Governance

- [x] 3.1 新增 readiness helper，支持短超时 TCP/backend dependency check。
- [x] 3.2 API Gateway `/ready` 改为使用统一 helper。
- [x] 3.3 定义 Game Gateway/Auth/Player/其他 TCP backend 的 ready 条件，先落最小可复用组件。
- [x] 3.4 ready 失败返回 503 和稳定错误码，不泄露敏感配置或签名细节。

## 4. Schema Migration

- [x] 4.1 新增 `schema_migrations` 表定义。
- [x] 4.2 新增 versioned migration 目录和初始 migration。
- [x] 4.3 新增 migration runner/helper，支持按版本执行、跳过已应用版本。
- [x] 4.4 已应用版本 checksum mismatch 必须失败。
- [x] 4.5 更新 `scripts/dev_up.sh`，保留 local reset 能力并接入 migration 验证路径。

## 5. Structured Logging

- [x] 5.1 扩展 `LogContext` 或 logging helper，统一 `event/service/request_id/trace_id/player_id/account_id/gateway_id/game_session_id/upstream/error_code/status/latency_ms` 字段。
- [x] 5.2 更新 Gateway/Auth/Player 等核心服务日志调用，使用统一字段。
- [x] 5.3 增加敏感字段脱敏规则，禁止日志输出 password、token、ticket、session_token、signature、secret 原文。

## 6. Probes And Flow

- [x] 6.1 新增 `production_runtime_governance_probe`。
- [x] 6.2 Probe 覆盖 metrics exporter 格式和指标名。
- [x] 6.3 Probe 覆盖 migration 重复执行跳过和 checksum mismatch 失败。
- [x] 6.4 Probe 覆盖 readiness reachable/unreachable。
- [x] 6.5 Probe 覆盖 structured logging 不泄露敏感字段。
- [x] 6.6 `scripts/run_local_flow.sh` 加入 `production_runtime_governance_probe`。

## 7. Verification

- [x] 7.1 `git diff --check`
- [x] 7.2 `cmake -S /home/love/code/mmo -B /home/love/code/mmo/build/production-runtime-governance -DCMAKE_BUILD_TYPE=Debug`
- [x] 7.3 `cmake --build /home/love/code/mmo/build/production-runtime-governance --parallel`
- [x] 7.4 `scripts/dev_up.sh`
- [x] 7.5 `production_runtime_governance_probe`
- [x] 7.6 `auth_data_probe`、`storage_governance_probe`、`security_governance_probe`、`internal_auth_probe`
- [x] 7.7 `scripts/run_local_flow.sh`
