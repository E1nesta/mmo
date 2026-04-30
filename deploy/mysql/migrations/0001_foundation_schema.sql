CREATE TABLE IF NOT EXISTS accounts (
  account_id BIGINT NOT NULL,
  account_name VARCHAR(64) NOT NULL,
  password_hash CHAR(64) NOT NULL,
  password_salt CHAR(32) NOT NULL,
  password_iterations INT NOT NULL,
  status VARCHAR(16) NOT NULL DEFAULT 'normal',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (account_id),
  UNIQUE KEY uniq_accounts_name (account_name),
  KEY idx_accounts_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS player_identities (
  account_id BIGINT NOT NULL,
  player_id BIGINT NOT NULL,
  is_primary TINYINT NOT NULL DEFAULT 1,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  deleted_at TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (account_id, player_id),
  UNIQUE KEY uniq_player_identities_player (player_id),
  KEY idx_player_identities_primary (account_id, is_primary, deleted_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS player_profiles (
  player_id BIGINT NOT NULL,
  gold BIGINT NOT NULL DEFAULT 0,
  exp BIGINT NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS reward_ledger (
  id BIGINT NOT NULL AUTO_INCREMENT,
  player_id BIGINT NOT NULL,
  idempotency_key VARCHAR(128) NOT NULL,
  request_id VARCHAR(128) NOT NULL,
  gold BIGINT NOT NULL DEFAULT 0,
  exp BIGINT NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uniq_reward_idempotency (idempotency_key),
  KEY idx_reward_player_created (player_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO accounts(
  account_id,
  account_name,
  password_hash,
  password_salt,
  password_iterations,
  status
) VALUES
  (
    1098216,
    'demo_player',
    'd8209a2c86e5d177389cbd88d1e7d836a98301af8e320d197249a4bb8012861b',
    '00112233445566778899aabbccddeeff',
    10000,
    'normal'
  ),
  (
    1098217,
    'demo_banned',
    'd8209a2c86e5d177389cbd88d1e7d836a98301af8e320d197249a4bb8012861b',
    '00112233445566778899aabbccddeeff',
    10000,
    'banned'
  ),
  (
    1098218,
    'demo_missing_identity',
    'd8209a2c86e5d177389cbd88d1e7d836a98301af8e320d197249a4bb8012861b',
    '00112233445566778899aabbccddeeff',
    10000,
    'normal'
  )
ON DUPLICATE KEY UPDATE
  password_hash=VALUES(password_hash),
  password_salt=VALUES(password_salt),
  password_iterations=VALUES(password_iterations),
  status=VALUES(status);

INSERT INTO player_identities(account_id, player_id, is_primary)
VALUES (1098216, 1198216, 1)
ON DUPLICATE KEY UPDATE is_primary=VALUES(is_primary), deleted_at=NULL;

INSERT INTO player_profiles(player_id, gold, exp)
VALUES (1198216, 0, 0)
ON DUPLICATE KEY UPDATE player_id=VALUES(player_id);
