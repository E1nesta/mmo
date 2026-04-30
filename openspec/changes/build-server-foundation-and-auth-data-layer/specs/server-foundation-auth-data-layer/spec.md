# Server Foundation Auth Data Layer

## ADDED Requirements

### Requirement: Auth server requires MySQL-backed identity data

`auth_server` SHALL initialize its MySQL storage dependency during startup and SHALL NOT fall back to mock or derived identity login.

#### Scenario: MySQL dependency is unavailable

- **WHEN** `auth_server` starts and the MySQL pool cannot be initialized
- **THEN** the process SHALL fail startup
- **AND** no mock login path SHALL be enabled.

#### Scenario: Login succeeds from stored identity

- **GIVEN** an active account with a valid password hash and primary player identity exists in MySQL
- **WHEN** Auth handles the login request
- **THEN** it SHALL return the stored `account_id` and `player_id`
- **AND** it SHALL issue a session token for that identity.

### Requirement: Auth data access is repository backed

Auth login SHALL read account and player identity data through repository boundaries.

#### Scenario: Account lookup

- **WHEN** Auth validates credentials
- **THEN** it SHALL use `AccountRepository` to load account id, account name, password hash, password salt, password iteration count, and account status.

#### Scenario: Player identity lookup

- **WHEN** an account has passed password and status checks
- **THEN** Auth SHALL use `PlayerIdentityRepository` to resolve the primary player identity.

### Requirement: Password verification uses PBKDF2-SHA256

Stored account passwords SHALL be verified using PBKDF2-SHA256 with per-account salt and iteration count.

#### Scenario: Correct password

- **GIVEN** the supplied password matches the stored PBKDF2-SHA256 hash
- **WHEN** Auth verifies the credentials
- **THEN** password verification SHALL pass.

#### Scenario: Incorrect password

- **GIVEN** the supplied password does not match the stored PBKDF2-SHA256 hash
- **WHEN** Auth verifies the credentials
- **THEN** password verification SHALL fail
- **AND** Auth SHALL NOT try a plaintext or demo fallback.

### Requirement: Login failures are client-generic

Auth login failures SHALL return a generic client error while preserving internal reasons for server-side logs and probes.

#### Scenario: Invalid login cases

- **WHEN** the account is missing, the password is wrong, the account is banned, or the player identity is missing
- **THEN** Auth SHALL return HTTP/RPC error code 401
- **AND** the client-visible message SHALL NOT reveal which condition occurred
- **AND** responses SHALL NOT contain raw passwords, tokens, or tickets.

### Requirement: Local schema seeds auth identities

The local MySQL schema SHALL include account and player identity tables with deterministic smoke credentials.

#### Scenario: Local smoke account

- **WHEN** local schema initialization runs
- **THEN** it SHALL create `accounts` and `player_identities`
- **AND** it SHALL seed `demo_player / demo_password` with stable `account_id` and `player_id`.

#### Scenario: Negative auth fixtures

- **WHEN** local schema initialization runs
- **THEN** it SHALL seed fixtures for a banned account and an account without player identity.

### Requirement: Player profile table naming is consistent

Player persistence SHALL use the plural `player_profiles` table name.

#### Scenario: Player repository reads profile

- **WHEN** `MysqlPlayerRepository` loads a player profile
- **THEN** it SHALL query `player_profiles`
- **AND** it SHALL NOT query `player_profile`.

### Requirement: API Gateway exposes dependency readiness

`api_gateway_server` SHALL keep `/health` for liveness and expose `/ready` for dependency readiness.

#### Scenario: Auth backend is reachable

- **WHEN** `/ready` is called and Auth backend TCP connectivity succeeds
- **THEN** API Gateway SHALL return a ready response.

#### Scenario: Auth backend is unavailable

- **WHEN** `/ready` is called and Auth backend TCP connectivity fails
- **THEN** API Gateway SHALL return a non-ready response.

### Requirement: Local flow boots storage dependencies

The local smoke workflow SHALL start and initialize MySQL/Redis dependencies before starting MMO services.

#### Scenario: Run local flow

- **WHEN** `scripts/run_local_flow.sh` runs
- **THEN** it SHALL start local storage dependencies
- **AND** replay the local MySQL schema
- **AND** run the Auth data probe before the end-to-end client flow.
