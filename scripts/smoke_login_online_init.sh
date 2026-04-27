#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/_common.sh"

LOGIN_INIT_RETRY_ATTEMPTS="${LOGIN_INIT_RETRY_ATTEMPTS:-5}"
LOGIN_INIT_RETRY_DELAY_SECONDS="${LOGIN_INIT_RETRY_DELAY_SECONDS:-2}"

run_demo_client_with_retry() {
  local output=""
  local attempt

  for attempt in $(seq 1 "$LOGIN_INIT_RETRY_ATTEMPTS"); do
    if output="$(run_demo_client "$@" 2>&1)"; then
      printf '%s\n' "$output"
      return 0
    fi

    if [[ "$attempt" -lt "$LOGIN_INIT_RETRY_ATTEMPTS" ]]; then
      printf 'login-online-init retry %s/%s after demo_client failure\n' \
        "$attempt" "$LOGIN_INIT_RETRY_ATTEMPTS" >&2
      printf '%s\n' "$output" >&2
      sleep "$LOGIN_INIT_RETRY_DELAY_SECONDS"
      continue
    fi
  done

  printf '%s\n' "$output" >&2
  return 1
}

build_local_binaries
# This focused smoke validates the real client-facing chain through the current
# compose image set, so rebuild containers by default to avoid binary/image drift.
COMPOSE_BUILD="${COMPOSE_BUILD:-1}" up_stack

LOGIN_OUTPUT="$(run_demo_client_with_retry --scenario login-only --happy-path-only "$@")"
SESSION_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^SESSION_ID=/{print $2}')"
PLAYER_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^PLAYER_ID=/{print $2}')"
ACCOUNT_ID="$(printf '%s\n' "$LOGIN_OUTPUT" | awk -F= '/^ACCOUNT_ID=/{print $2}')"

if [[ -z "$SESSION_ID" || -z "$PLAYER_ID" || -z "$ACCOUNT_ID" ]]; then
  echo "failed to parse login-only output during login/online/init smoke check" >&2
  printf '%s\n' "$LOGIN_OUTPUT" >&2
  exit 1
fi

run_demo_client_with_retry \
  --scenario load-only \
  --no-reset \
  --session-id "$SESSION_ID" \
  --player-id "$PLAYER_ID" >/dev/null

echo "login/online/init smoke completed successfully"
echo "ACCOUNT_ID=$ACCOUNT_ID"
echo "PLAYER_ID=$PLAYER_ID"
echo "SESSION_ID=$SESSION_ID"
