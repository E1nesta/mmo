#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/local-flow"
export MMO_CONFIG_PATH="${ROOT_DIR}/configs/local/server.yaml"
export MMO_MYSQL_PASSWORD="${MMO_MYSQL_PASSWORD:-mmo_dev_local}"
export MMO_REDIS_PASSWORD="${MMO_REDIS_PASSWORD:-}"

PIDS=()

cleanup() {
  for pid in "${PIDS[@]:-}"; do
    if kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" >/dev/null 2>&1 || true
      wait "${pid}" >/dev/null 2>&1 || true
    fi
  done
}
trap cleanup EXIT

wait_for_port() {
  local port="$1"
  for _ in $(seq 1 50); do
    if python3 - "${port}" <<'PY' >/dev/null 2>&1
import socket
import sys

port = int(sys.argv[1])
with socket.create_connection(("127.0.0.1", port), timeout=0.1):
    pass
PY
    then
      return 0
    fi
    sleep 0.1
  done
  echo "timeout waiting for port ${port}" >&2
  return 1
}

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${BUILD_DIR}" --parallel

"${ROOT_DIR}/scripts/dev_up.sh"

"${BUILD_DIR}/auth_server" &
PIDS+=("$!")
"${BUILD_DIR}/api_gateway_server" &
PIDS+=("$!")
"${BUILD_DIR}/scene_server" &
PIDS+=("$!")
"${BUILD_DIR}/world_server" &
PIDS+=("$!")
"${BUILD_DIR}/instance_server" &
PIDS+=("$!")
"${BUILD_DIR}/player_server" &
PIDS+=("$!")
"${BUILD_DIR}/social_server" &
PIDS+=("$!")
"${BUILD_DIR}/game_gateway_server" &
PIDS+=("$!")

wait_for_port 4101
wait_for_port 4100
wait_for_port 4104
wait_for_port 4103
wait_for_port 4105
wait_for_port 4106
wait_for_port 4107
wait_for_port 4102

"${BUILD_DIR}/internal_auth_probe"
"${BUILD_DIR}/auth_data_probe"
"${BUILD_DIR}/storage_governance_probe"
"${BUILD_DIR}/production_runtime_governance_probe"
"${BUILD_DIR}/gateway_session_governance_probe"
"${BUILD_DIR}/cluster_routing_governance_probe"
"${BUILD_DIR}/mmo_flow_client"
