#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/_common.sh"

build_local_binaries
BUILD_DIR="$(build_dir)"

"$BUILD_DIR/api_gateway_server" --config configs/api_gateway_server.conf --check
"$BUILD_DIR/online_gateway_server" --config configs/online_gateway_server.conf --check
"$BUILD_DIR/auth_server" --config configs/auth_server.conf --check
"$BUILD_DIR/player_query_server" --config configs/player_query_server.conf --check
"$BUILD_DIR/player_write_grpc_server" --config configs/player_write_grpc_server.conf --check
"$BUILD_DIR/dungeon_runtime_server" --config configs/dungeon_runtime_server.conf --check
"$BUILD_DIR/social_server" --config configs/social_server.conf --check

SKIP_BUILD=1 ./scripts/run_network_demo.sh
