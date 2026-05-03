#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

fail() {
    printf 'architecture boundary check failed: %s\n' "$1" >&2
    exit 1
}

if rg -n '#include "modules/' runtime >/tmp/mmo_runtime_module_includes.txt; then
    cat /tmp/mmo_runtime_module_includes.txt >&2
    fail "runtime must not include modules"
fi

if rg -n '#include "modules/.+service\.h"' modules --glob '*repository*.h' \
    >/tmp/mmo_repository_service_includes.txt; then
    cat /tmp/mmo_repository_service_includes.txt >&2
    fail "repository headers must not include service headers"
fi

backend_public_handlers="$(
    rg -n 'public\.|k(Login|GateLogin|Ping|Reconnect|EnterWorld|EnterScene|Move|CastSkill|EnterInstance|SettleInstance|ApplyReward|SocialBoundary)' \
        apps/auth_server \
        apps/world_server \
        apps/scene_server \
        apps/instance_server \
        apps/player_server \
        apps/social_server || true
)"
if [[ -n "${backend_public_handlers}" ]]; then
    printf '%s\n' "${backend_public_handlers}" >&2
    fail "backend services must not register client public handlers"
fi

printf 'architecture boundary check ok\n'
