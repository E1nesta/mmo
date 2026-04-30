# Storage Governance Redis Session Runtime

## ADDED Requirements

### Requirement: Player server persists rewards through MySQL

`player_server` SHALL use a MySQL-backed player repository for long-lived player reward writes.

#### Scenario: Player server starts without MySQL

- **WHEN** `player_server` cannot initialize its MySQL pool
- **THEN** startup SHALL fail
- **AND** reward writes SHALL NOT fall back to memory-only production behavior.

#### Scenario: Reward applies once

- **GIVEN** a player reward request has a new idempotency key
- **WHEN** Player Service applies the reward
- **THEN** it SHALL insert a `reward_ledger` row
- **AND** update `player_profiles` in the same MySQL transaction
- **AND** return the updated profile values.

#### Scenario: Duplicate reward request

- **GIVEN** a reward idempotency key has already been recorded
- **WHEN** the same reward request is applied again
- **THEN** Player Service SHALL NOT apply the reward twice
- **AND** it SHALL return `applied=false` with the current profile values.

### Requirement: Gateway ticket replay is Redis-backed

Game Gateway SHALL use Redis to enforce one-time gateway ticket consumption across gateway processes.

#### Scenario: First ticket consumption

- **GIVEN** a valid gateway ticket jti has not been consumed
- **WHEN** GateLogin consumes the ticket
- **THEN** Redis SHALL record the jti with a TTL bounded by the ticket expiry
- **AND** GateLogin MAY proceed.

#### Scenario: Ticket replay

- **GIVEN** a gateway ticket jti has already been consumed
- **WHEN** GateLogin receives the same ticket again
- **THEN** Redis-backed replay protection SHALL reject it
- **AND** GateLogin SHALL fail.

### Requirement: Game session and online state are Redis-backed

Game Gateway SHALL write authenticated game session and online state to Redis.

#### Scenario: GateLogin success

- **WHEN** GateLogin validates the gateway ticket and creates a local binding
- **THEN** Game Gateway SHALL write `game_session:{game_session_id}`
- **AND** it SHALL write `online:{player_id}`
- **AND** both keys SHALL have TTLs aligned with the game session expiry.

#### Scenario: Heartbeat touch

- **WHEN** an authenticated Ping refreshes heartbeat
- **THEN** Game Gateway SHALL update session `last_seen`
- **AND** refresh Redis TTLs for session and online keys.

#### Scenario: Session-bound request

- **WHEN** Game Gateway handles a session-bound public request
- **THEN** it SHALL validate both local binding and Redis session binding.

### Requirement: Redis is required for Game Gateway session governance

`game_gateway_server` SHALL initialize Redis before accepting public traffic.

#### Scenario: Redis unavailable

- **WHEN** Redis pool initialization fails during Game Gateway startup
- **THEN** `game_gateway_server` SHALL fail startup.
