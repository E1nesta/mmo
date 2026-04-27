#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/_common.sh"

LOG_WINDOW="${DEMO_LOG_WINDOW:-10m}"
SMOKE_RETRY_ATTEMPTS="${SMOKE_RETRY_ATTEMPTS:-5}"
SMOKE_RETRY_DELAY_SECONDS="${SMOKE_RETRY_DELAY_SECONDS:-2}"

wait_for_log_pattern() {
  local pattern="$1"
  shift

  local attempt
  for attempt in $(seq 1 10); do
    local logs
    logs="$(compose_cmd logs --since "$LOG_WINDOW" "$@" 2>/dev/null || true)"
    if grep -q "$pattern" <<<"$logs"; then
      return 0
    fi
    sleep 1
  done

  return 1
}

run_demo_client_with_retry() {
  local output=""
  local attempt

  for attempt in $(seq 1 "$SMOKE_RETRY_ATTEMPTS"); do
    if output="$(run_demo_client "$@" 2>&1)"; then
      printf '%s\n' "$output"
      return 0
    fi

    if [[ "$attempt" -lt "$SMOKE_RETRY_ATTEMPTS" ]]; then
      printf 'smoke retry %s/%s after demo_client failure\n' "$attempt" "$SMOKE_RETRY_ATTEMPTS" >&2
      printf '%s\n' "$output" >&2
      sleep "$SMOKE_RETRY_DELAY_SECONDS"
      continue
    fi
  done

  printf '%s\n' "$output" >&2
  return 1
}

build_local_binaries
# Smoke must validate the current source tree end-to-end, including nginx stream
# and online gateway transport behavior inside containers. Default to rebuilding
# the compose image so local binaries and in-container services do not drift.
COMPOSE_BUILD="${COMPOSE_BUILD:-1}" up_stack

run_demo_client_with_retry --happy-path-only "$@"
LOGIN_OUTPUT="$(run_demo_client_with_retry --scenario login-only --happy-path-only)"
SESSION_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^SESSION_ID=/{print $2}')"
PLAYER_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^PLAYER_ID=/{print $2}')"

if [[ -z "$SESSION_ID" || -z "$PLAYER_ID" ]]; then
  echo "failed to parse login-only output during smoke check" >&2
  printf '%s\n' "$LOGIN_OUTPUT" >&2
  exit 1
fi

run_demo_client_with_retry --scenario load-only --no-reset --session-id "$SESSION_ID" --player-id "$PLAYER_ID" >/dev/null

echo "smoke checks completed successfully"
