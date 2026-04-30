# MMO Runtime Production Infrastructure

## ADDED Requirements

### Requirement: MMO hot path remains self-developed runtime

The project SHALL keep MMO-specific hot-path capabilities inside the self-developed C++ runtime.

#### Scenario: Gameplay main path architecture review

- **WHEN** Gateway routing, game session, service-to-service game RPC, sharded execution, Scene ownership, AOI, movement, combat, overload protection, or realtime transport is changed
- **THEN** the design SHALL use project runtime boundaries such as `runtime/transport`, `runtime/rpc`, `runtime/channel`, `runtime/routing`, `runtime/session`, and `runtime/execution`
- **AND** it SHALL preserve player/scene ownership semantics.

### Requirement: Production infrastructure uses mainstream systems

The project SHALL use mainstream infrastructure for production concerns that are not MMO-specific runtime differentiation.

#### Scenario: Production deployment architecture review

- **WHEN** TLS termination, public edge routing, database high availability, Redis high availability, schema migration, metrics, tracing, logging, secrets, or orchestration is planned
- **THEN** the architecture SHALL prefer mature infrastructure such as Nginx/Envoy/Cloud LB, MySQL HA, Redis HA, Flyway/Liquibase, Prometheus, OpenTelemetry, Grafana/Loki, Docker, or Kubernetes
- **AND** the project SHALL integrate with these systems rather than reimplementing them as game runtime code.

### Requirement: Generic frameworks do not replace the game main path by default

Generic RPC, service mesh, message queue, or actor frameworks SHALL NOT replace the game main path without a dedicated OpenSpec change.

#### Scenario: Propose gRPC, brpc, service mesh, MQ, or actor runtime

- **WHEN** a change proposes introducing gRPC, brpc, service mesh, Kafka/RocketMQ/NATS, or a full Actor framework on the game main path
- **THEN** the change SHALL document the exact path being replaced
- **AND** it SHALL analyze effects on long connections, gateway session, player shard routing, Scene ownership, performance, operational complexity, and rollback.

### Requirement: Edge/LB is deployment infrastructure, not game gateway logic

Edge/LB components SHALL not own MMO gameplay or session semantics.

#### Scenario: Edge proxy placement

- **WHEN** public traffic enters through Nginx, Envoy, or a cloud load balancer
- **THEN** those components MAY terminate TLS, route L4/L7 traffic, and apply coarse rate limits
- **BUT** they SHALL NOT own game session state, public/internal protocol conversion, player routing, AOI, movement simulation, or combat resolution.

### Requirement: Implementation order follows runtime and production readiness

The next infrastructure stages SHALL prioritize observability, migration, readiness, session governance, and load validation before gameplay breadth.

#### Scenario: Roadmap review

- **WHEN** selecting the next framework task
- **THEN** Prometheus metrics exporter, schema migration/versioning, dependency readiness, duplicate login/reconnect/session takeover, and TCP load testing SHALL be prioritized ahead of additional gameplay CRUD.
