#include "runtime/net/frame_transport.h"

namespace runtime::net {

TransportStats snapshot_transport_counters(const TransportCounters& counters) {
    TransportStats stats;
    stats.active_connections = counters.active_connections.load();
    stats.accepted_connections = counters.accepted_connections.load();
    stats.rejected_connections = counters.rejected_connections.load();
    stats.closed_connections = counters.closed_connections.load();
    stats.read_frames = counters.read_frames.load();
    stats.written_frames = counters.written_frames.load();
    stats.read_bytes = counters.read_bytes.load();
    stats.written_bytes = counters.written_bytes.load();
    stats.errors = counters.errors.load();
    return stats;
}

}  // namespace runtime::net
