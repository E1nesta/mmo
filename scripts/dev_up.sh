#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"
export MMO_MYSQL_PORT="${MMO_MYSQL_PORT:-13306}"
export MMO_MYSQL_PASSWORD="${MMO_MYSQL_PASSWORD:-mmo_dev_local}"
docker compose up -d mysql redis

wait_for_healthy() {
  local container="$1"
  for _ in $(seq 1 60); do
    local status
    status="$(docker inspect -f '{{.State.Health.Status}}' "${container}" 2>/dev/null || true)"
    if [[ "${status}" == "healthy" ]]; then
      return 0
    fi
    sleep 1
  done
  echo "timeout waiting for ${container} to become healthy" >&2
  return 1
}

wait_for_healthy mmo-mysql
wait_for_healthy mmo-redis

docker compose exec -T mysql mysql \
  -u"${MMO_MYSQL_USER:-mmo}" \
  -p"${MMO_MYSQL_PASSWORD}" \
  "${MMO_MYSQL_DATABASE:-mmo_local}" \
  < "${ROOT_DIR}/deploy/mysql/init/001_schema.sql"
