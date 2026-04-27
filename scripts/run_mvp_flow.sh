#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/_common.sh"

COMPOSE_BUILD="${COMPOSE_BUILD:-1}" up_stack

build_mvp_binaries
run_demo_client --happy-path-only
run_demo_client --scenario login-only --happy-path-only
