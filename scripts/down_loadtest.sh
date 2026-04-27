#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
export COMPOSE_ENV_FILE="$ROOT_DIR/deploy/.env.demo.loadtest"

source "$ROOT_DIR/scripts/_common.sh"

compose_cmd down --remove-orphans
