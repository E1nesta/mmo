#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/_common.sh"

STOPPED_SERVICES=()

run_with_retry() {
  local attempts="$1"
  shift

  local try=1
  while (( try <= attempts )); do
    if "$@"; then
      return 0
    fi

    if (( try == attempts )); then
      return 1
    fi

    sleep 1
    ((try++))
  done
}

wait_service() {
  local service="$1"
  local container_id
  local state
  local health

  compose_cmd start "$service" >/dev/null
  container_id="$(compose_cmd ps -q "$service")"
  if [[ -z "$container_id" ]]; then
    echo "failed to resolve container id for $service" >&2
    return 1
  fi

  for _ in $(seq 1 60); do
    state="$(docker inspect --format '{{.State.Status}}' "$container_id")"
    health="$(docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "$container_id")"
    if [[ "$state" == "running" && ( "$health" == "healthy" || "$health" == "none" ) ]]; then
      return 0
    fi
    sleep 1
  done

  echo "service $service did not become healthy in time" >&2
  return 1
}

mark_stopped() {
  STOPPED_SERVICES+=("$1")
}

cleanup() {
  local service
  for (( idx=${#STOPPED_SERVICES[@]}-1; idx>=0; idx-- )); do
    service="${STOPPED_SERVICES[idx]}"
    wait_service "$service" || true
  done
}

trap cleanup EXIT

echo "[1/3] failover: stop online_gateway_1 and verify gateway login/relink still succeeds"
build_local_binaries
COMPOSE_BUILD="${COMPOSE_BUILD:-0}" up_stack

compose_cmd stop online_gateway_1 >/dev/null
mark_stopped online_gateway_1
CLIENT_TIMEOUT_MS=5000 run_with_retry 5 run_demo_client --happy-path-only --skip-session-recovery
wait_service online_gateway_1

echo "[2/3] upstream error: stop dungeon_runtime_server and expect SERVICE_UNAVAILABLE on enter"
compose_cmd stop dungeon_runtime_server >/dev/null
mark_stopped dungeon_runtime_server
run_demo_client --happy-path-only --skip-session-recovery --expect-enter-error SERVICE_UNAVAILABLE
wait_service dungeon_runtime_server

echo "[3/3] session expiry: delete session and expect gate relink/player init to fail with SESSION_INVALID"
LOGIN_OUTPUT="$(run_demo_client --scenario login-only)"
SESSION_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^SESSION_ID=/{print $2}')"
ACCOUNT_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^ACCOUNT_ID=/{print $2}')"
PLAYER_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^PLAYER_ID=/{print $2}')"

if [[ -z "$SESSION_ID" || -z "$ACCOUNT_ID" || -z "$PLAYER_ID" ]]; then
  echo "failed to parse login-only output" >&2
  printf '%s\n' "$LOGIN_OUTPUT" >&2
  exit 1
fi

compose_cmd exec -T redis redis-cli DEL "session:$SESSION_ID" "account:session:$ACCOUNT_ID" >/dev/null
run_with_retry 3 run_demo_client --scenario load-only --session-id "$SESSION_ID" --player-id "$PLAYER_ID" --expect-load-error SESSION_INVALID

echo "gateway fault checks completed successfully"
