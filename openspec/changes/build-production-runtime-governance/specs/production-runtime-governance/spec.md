# Production Runtime Governance

## ADDED Requirements

### Requirement: Services expose Prometheus-compatible runtime metrics

The runtime SHALL provide a Prometheus text format exporter for in-memory `MetricsSnapshot` values.

#### Scenario: Metrics snapshot is exported

- **GIVEN** a `MetricsRegistry` contains runtime counters and gauges
- **WHEN** the metrics exporter renders a snapshot
- **THEN** the output SHALL use Prometheus text exposition format
- **AND** every metric name SHALL use the `mmo_` prefix
- **AND** counters SHALL be exposed with `_total` names
- **AND** gauges SHALL represent current values without querying MySQL, Redis, or backend services.

#### Scenario: API Gateway metrics endpoint

- **WHEN** an operator sends `GET /metrics` to `api_gateway_server`
- **THEN** the response SHALL return Prometheus text format
- **AND** include login, gateway ticket, request, error, and runtime metrics available in its `MetricsRegistry`.

### Requirement: Readiness is dependency-aware and separate from liveness

Services SHALL distinguish liveness from readiness.

#### Scenario: Health endpoint

- **WHEN** an operator calls `/health`
- **THEN** the service SHALL report process liveness
- **AND** it SHALL NOT perform blocking MySQL, Redis, or backend dependency checks.

#### Scenario: Ready endpoint succeeds

- **GIVEN** all required dependencies for a service are reachable within the configured timeout
- **WHEN** `/ready` or an equivalent readiness probe is evaluated
- **THEN** readiness SHALL succeed.

#### Scenario: Ready endpoint fails

- **GIVEN** a required dependency is unavailable or times out
- **WHEN** readiness is evaluated
- **THEN** readiness SHALL fail with 503 or an equivalent not-ready result
- **AND** the response SHALL contain a stable error code
- **AND** it SHALL NOT expose secrets, signatures, tokens, tickets, or passwords.

### Requirement: Database schema changes are versioned

MySQL schema evolution SHALL use versioned migrations.

#### Scenario: First migration run

- **GIVEN** an empty database with no applied migration records
- **WHEN** the migration runner executes
- **THEN** it SHALL create or verify `schema_migrations`
- **AND** apply migrations in version order
- **AND** record version, name, checksum, and applied time.

#### Scenario: Repeated migration run

- **GIVEN** migrations were already applied with matching checksums
- **WHEN** the migration runner executes again
- **THEN** it SHALL skip already applied versions
- **AND** complete successfully without duplicating schema objects or seed rows.

#### Scenario: Applied migration checksum changes

- **GIVEN** a migration version is already recorded
- **AND** the migration file checksum no longer matches the recorded checksum
- **WHEN** the migration runner evaluates that version
- **THEN** migration SHALL fail
- **AND** the runner SHALL NOT silently overwrite schema history.

### Requirement: Structured logs use a stable runtime context

Core services SHALL emit structured logs with stable field names for production debugging.

#### Scenario: Core service event is logged

- **WHEN** API Gateway, Game Gateway, Auth Server, or Player Server logs a request or important state transition
- **THEN** the log SHALL include available context fields from `event`, `service`, `request_id`, `trace_id`, `player_id`, `account_id`, `gateway_id`, `game_session_id`, `upstream`, `error_code`, `status`, and `latency_ms`
- **AND** missing optional values MAY be omitted or left empty.

#### Scenario: Sensitive values are present in runtime data

- **GIVEN** a request contains password, password hash, access token, gateway ticket, session token, internal signature, or shared secret
- **WHEN** a service writes structured logs
- **THEN** those sensitive raw values SHALL NOT appear in logs.

### Requirement: Runtime governance is locally verifiable

The repository SHALL include a local probe that validates production runtime governance behavior.

#### Scenario: Governance probe passes

- **WHEN** `production_runtime_governance_probe` runs against local dependencies
- **THEN** it SHALL verify metrics exporter formatting
- **AND** verify readiness success and failure behavior
- **AND** verify migration idempotency and checksum mismatch failure
- **AND** verify structured logging does not leak sensitive values.

#### Scenario: Local flow includes governance probe

- **WHEN** `scripts/run_local_flow.sh` runs
- **THEN** it SHALL execute `production_runtime_governance_probe`
- **AND** continue to validate the existing login, gate login, enter world, enter instance, settle instance, social boundary, and flow ok smoke path.
