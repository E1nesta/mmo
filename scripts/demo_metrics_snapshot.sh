#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/scripts/_common.sh"

WINDOW="${METRICS_WINDOW:-60s}"
WITH_SMOKE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --window)
      if [[ $# -lt 2 ]]; then
        echo "missing value for --window" >&2
        exit 1
      fi
      WINDOW="$2"
      shift 2
      ;;
    --with-smoke)
      WITH_SMOKE=1
      shift
      ;;
    *)
      echo "unsupported argument: $1" >&2
      exit 1
      ;;
  esac
done

if ! docker_cmd >/dev/null 2>&1; then
  echo "docker is required for metrics snapshots but is not available in the current environment" >&2
  exit 1
fi

if ! RUNNING_CONTAINERS="$(compose_cmd ps -q online_gateway_1 online_gateway_2 api_gateway_server auth_server player_query_server player_write_grpc_server dungeon_runtime_server social_server 2>/dev/null)"; then
  echo "docker compose is not available for profile '$DEPLOY_PROFILE' in the current environment" >&2
  exit 1
fi

if [[ -z "${RUNNING_CONTAINERS//[$'\t\r\n ']}" ]]; then
  echo "no running compose stack detected for profile '$DEPLOY_PROFILE'" >&2
  echo "start the stack first with ./scripts/up.sh or DEPLOY_PROFILE=delivery ./scripts/up.sh" >&2
  exit 1
fi

if [[ "$WITH_SMOKE" == "1" ]]; then
  echo "running smoke checks before capturing metrics snapshot"
  bash "$ROOT_DIR/scripts/smoke.sh"
fi

echo "capturing metrics snapshot for profile '$DEPLOY_PROFILE' over window '$WINDOW'"
python3 "$ROOT_DIR/scripts/metrics_snapshot.py" --window "$WINDOW"
