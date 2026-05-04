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

runtime_app_protocol_refs="$(
    rg -n '#include "apps/protocol/message_types\.h"|mmo::apps::protocol::k' \
        runtime || true
)"
if [[ -n "${runtime_app_protocol_refs}" ]]; then
    printf '%s\n' "${runtime_app_protocol_refs}" >&2
    fail "runtime must not depend on app-level business message catalog"
fi

runtime_protocol_business_catalog="$(
    rg -n '"(public|internal)\.' runtime/protocol || true
)"
if [[ -n "${runtime_protocol_business_catalog}" ]]; then
    printf '%s\n' "${runtime_protocol_business_catalog}" >&2
    fail "runtime/protocol must not define business message type catalog"
fi

legacy_framework_refs="$(
    rg -n 'runtime/routing|mmo::runtime::routing|runtime/rpc/rpc_server_app|runtime/foundation/server_app|runtime_routing|\bForwardResult\b|\bRouteTarget\b|\bRouteTable\b' \
        apps runtime modules tools CMakeLists.txt || true
)"
if [[ -n "${legacy_framework_refs}" ]]; then
    printf '%s\n' "${legacy_framework_refs}" >&2
    fail "legacy routing/server framework names must not remain"
fi

if rg -n '#include "modules/.+service\.h"' modules --glob '*repository*.h' \
    >/tmp/mmo_repository_service_includes.txt; then
    cat /tmp/mmo_repository_service_includes.txt >&2
    fail "repository headers must not include service headers"
fi

backend_public_handlers="$(
    rg -n '#include "public/|public\.|k(Login|GateLogin|Ping|Reconnect|EnterWorld|EnterScene|Move|CastSkill|EnterInstance|SettleInstance|ApplyReward|SocialBoundary)' \
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

runtime_session_redis_adapter_refs="$(
    rg -n 'Redis(SessionStore|TicketReplayStore)|redis_session_store|redis_ticket_replay_store' \
        runtime/session || true
)"
if [[ -n "${runtime_session_redis_adapter_refs}" ]]; then
    printf '%s\n' "${runtime_session_redis_adapter_refs}" >&2
    fail "Redis session adapters must live outside runtime/session core"
fi

printf 'architecture boundary check ok\n'
