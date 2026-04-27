#include "runtime/session/in_memory_session_store.h"
#include "apps/gateway/session_binding_service.h"

#include <iostream>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main() {
    common::session::InMemorySessionStore session_repository;
    const auto session = session_repository.Create(10001, 20001);
    services::gateway::SessionBindingService store(session_repository);

    common::net::RequestContext context;
    context.auth_token = session.session_id;

    const auto restored = store.ValidateOrRestore(1, &context);
    if (!Expect(restored.status == services::gateway::SessionBindingService::Status::kRestored,
                "expected first request to restore binding from session")) {
        return 1;
    }
    if (!Expect(context.player_id == session.player_id,
                "expected restore to backfill player_id into request context")) {
        return 1;
    }
    if (!Expect(context.account_id == session.account_id,
                "expected restore to backfill account_id into request context")) {
        return 1;
    }

    common::net::RequestContext rebound_context;
    rebound_context.auth_token = session.session_id;
    const auto rebound = store.ValidateOrRestore(1, &rebound_context);
    if (!Expect(rebound.status == services::gateway::SessionBindingService::Status::kBound,
                "expected second request to use local binding")) {
        return 1;
    }
    if (!Expect(rebound_context.player_id == session.player_id,
                "expected local binding to backfill player_id into request context")) {
        return 1;
    }
    store.UpdateClientMetadata(1, 7, 1, "device-a", "client-session-a");
    const auto by_connection = store.FindByConnectionId(1);
    if (!Expect(by_connection.has_value() && by_connection->line_no == 7 &&
                        by_connection->device_id == "device-a" &&
                        by_connection->client_session_id == "client-session-a",
                "expected metadata update to enrich the online binding")) {
        return 1;
    }
    if (!Expect(store.Touch(1), "expected heartbeat touch to succeed for bound connection")) {
        return 1;
    }
    const auto by_player = store.FindByPlayerId(session.player_id);
    if (!Expect(by_player.has_value() && by_player->auth_token == session.session_id,
                "expected player lookup to resolve the active connection binding")) {
        return 1;
    }
    const auto connection_id = store.FindConnectionIdByPlayerId(session.player_id);
    if (!Expect(connection_id.has_value() && *connection_id == 1,
                "expected player connection lookup to resolve the active connection id")) {
        return 1;
    }

    common::net::RequestContext invalid_context = context;
    invalid_context.player_id = 99999;
    const auto invalid = store.ValidateOrRestore(1, &invalid_context);
    if (!Expect(invalid.status == services::gateway::SessionBindingService::Status::kInvalid,
                "expected mismatched binding to fail")) {
        return 1;
    }

    store.Unbind(1);
    common::net::RequestContext missing_context;
    missing_context.auth_token = "missing";
    missing_context.player_id = 20001;
    const auto missing_session = store.ValidateOrRestore(2, &missing_context);
    if (!Expect(missing_session.status == services::gateway::SessionBindingService::Status::kInvalid,
                "expected missing session to fail restore")) {
        return 1;
    }

    common::net::RequestContext takeover_context;
    takeover_context.auth_token = session.session_id;
    const auto takeover = store.ValidateOrRestore(3, &takeover_context);
    if (!Expect(takeover.status == services::gateway::SessionBindingService::Status::kRestored,
                "expected new connection to restore the session")) {
        return 1;
    }
    if (!Expect(!store.FindByConnectionId(1).has_value(),
                "expected relink-style takeover to evict the previous connection binding")) {
        return 1;
    }
    if (!Expect(store.FindByConnectionId(3).has_value(),
                "expected takeover connection to become the active binding")) {
        return 1;
    }
    const auto takeover_connection_id = store.FindConnectionIdByPlayerId(session.player_id);
    if (!Expect(takeover_connection_id.has_value() && *takeover_connection_id == 3,
                "expected takeover connection id to replace the old binding")) {
        return 1;
    }

    if (!Expect(session_repository.BindDeviceId(session.session_id, "device-b"),
                "expected binding a device id onto the session to succeed")) {
        return 1;
    }

    if (!Expect(session_repository.RevokeById(session.session_id),
                "expected session revocation to succeed")) {
        return 1;
    }

    common::net::RequestContext revoked_context;
    revoked_context.auth_token = session.session_id;
    const auto revoked = store.ValidateOrRestore(4, &revoked_context);
    if (!Expect(revoked.status == services::gateway::SessionBindingService::Status::kInvalid,
                "expected revoked session restore to fail")) {
        return 1;
    }

    const auto revoked_bound = store.ValidateBoundSession(3);
    if (!Expect(revoked_bound.status == services::gateway::SessionBindingService::Status::kInvalid,
                "expected bound connection validation to fail after session revocation")) {
        return 1;
    }

    return 0;
}
