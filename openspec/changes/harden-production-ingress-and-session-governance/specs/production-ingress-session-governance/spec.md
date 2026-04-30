# Production Ingress Session Governance

## ADDED Requirements

### Requirement: HTTPS is terminated at Edge/LB

Production public HTTP API traffic SHALL use HTTPS terminated by Edge/LB, while `api_gateway_server` remains an internal HTTP upstream.

#### Scenario: Production API ingress

- **GIVEN** a client calls a public API
- **WHEN** the request enters production infrastructure
- **THEN** HTTPS/TLS SHALL terminate at Edge/LB
- **AND** Edge/LB SHALL forward to `api_gateway_server` over internal network
- **AND** certificate private keys SHALL NOT be stored in the repository.

### Requirement: Backend services remain internal-only

Backend business services SHALL remain unavailable as public client ingress.

#### Scenario: Service exposure review

- **WHEN** production ingress is reviewed
- **THEN** public exposure SHALL be limited to Edge/LB, API Gateway, and Game Gateway
- **AND** Auth, World, Scene, Instance, Player, and Social services SHALL require internal proto and signed internal RPC.

### Requirement: Tokens and tickets are governed

Access tokens and gateway tickets SHALL include enough claims to support audience restriction, key rotation, expiry, and replay defense.

#### Scenario: Token format review

- **WHEN** `access_token` or `gateway_ticket` is issued
- **THEN** it SHALL include version, key id, issuer, audience, purpose, subject identifiers, issued time, expiry time, nonce or jti, and signature
- **AND** token validation SHALL check purpose, audience, expiry, and signature key id.

#### Scenario: Key rotation

- **GIVEN** active and previous signing keys are configured
- **WHEN** token validation runs
- **THEN** new tokens SHALL be signed with the active key
- **AND** previous key validation SHALL only be accepted during the configured rotation window.

### Requirement: Gateway tickets are replay protected

Gateway admission tickets SHALL be short lived and SHALL NOT be reusable after successful consumption.

#### Scenario: Ticket replay

- **GIVEN** a valid `gateway_ticket` has already been consumed by GateLogin
- **WHEN** the same ticket is used again
- **THEN** Game Gateway SHALL reject the request
- **AND** record a replay metric
- **AND** it SHALL NOT create or replace a game session.

### Requirement: Game sessions are first-class connection state

Game Gateway SHALL create and validate a `game_session_id` after successful GateLogin.

#### Scenario: GateLogin success

- **WHEN** GateLogin validates `gateway_ticket`
- **THEN** Game Gateway SHALL create `game_session_id`
- **AND** bind it to connection id, gateway id, account id, player id, session token or auth session id, last seen time, and expiry time.

#### Scenario: Session-bound request

- **GIVEN** a request requires a bound game session
- **WHEN** Game Gateway handles the request
- **THEN** it SHALL validate the game session
- **AND** reject missing, expired, or mismatched session state.

### Requirement: Heartbeat, reconnect, and duplicate login are governed

Game Gateway SHALL have deterministic behavior for heartbeat timeout, reconnect, and duplicate login.

#### Scenario: Heartbeat timeout

- **GIVEN** a game session has not sent heartbeat within the configured timeout
- **WHEN** a session-bound request arrives
- **THEN** Game Gateway SHALL expire the session
- **AND** reject the request.

#### Scenario: Duplicate login

- **GIVEN** a player already has a bound game session
- **WHEN** another valid GateLogin for the same player succeeds
- **THEN** the new session SHALL replace the old session
- **AND** the old session SHALL be unbound.

### Requirement: Security observability avoids secret leakage

Ingress and session security events SHALL be observable without logging secrets.

#### Scenario: Security event logging

- **WHEN** login, ticket validation, GateLogin, reconnect, session expiry, or internal auth failure occurs
- **THEN** logs and metrics SHALL include request id, trace id, player id when known, gateway id, game session id when known, error code, and event name
- **AND** logs SHALL NOT include raw tokens, raw tickets, or full signatures.

### Requirement: KCP and realtime remain postponed

KCP/UDP realtime runtime SHALL remain out of this production ingress hardening stage.

#### Scenario: Realtime scope review

- **WHEN** this change is implemented
- **THEN** no new `realtime_gateway_server` runtime SHALL be added
- **AND** no RealtimeBind, movement, AOI, combat, or scene realtime sync path SHALL be added
- **AND** realtime endpoint fields, if returned by API Gateway, SHALL be treated as reserved or disabled until the realtime phase.
