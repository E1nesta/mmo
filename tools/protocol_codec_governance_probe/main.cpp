#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "runtime/protocol/message_catalog.h"
#include "runtime/net/realtime_packet_codec.h"
#include "runtime/net/reliable_frame_codec.h"

namespace {

void verify_message_catalog() {
    runtime::protocol::MessageCatalog catalog;
    catalog.add({
        10010,
        "mmo.cs.GateLoginRequest",
        runtime::protocol::MessageDomain::kPublicReliable,
        runtime::protocol::kMessageTransportTcp,
        runtime::protocol::MessageMode::kCall,
    });
    catalog.add({
        20000,
        "mmo.cs.MoveCommand",
        runtime::protocol::MessageDomain::kPublicRealtime,
        runtime::protocol::kMessageTransportKcp,
        runtime::protocol::MessageMode::kCast,
    });

    const auto* gate_login = catalog.find_by_id(10010);
    assert(gate_login != nullptr);
    assert(gate_login->message_type == "mmo.cs.GateLoginRequest");
    assert(gate_login->domain ==
           runtime::protocol::MessageDomain::kPublicReliable);
    assert(runtime::protocol::message_transport_allowed(
        gate_login->allowed_transports,
        runtime::protocol::kMessageTransportTcp));
    assert(runtime::protocol::message_transport_allowed(
        runtime::protocol::default_transports_for_domain(
            runtime::protocol::MessageDomain::kPublicReliable),
        runtime::protocol::kMessageTransportInternal));
    assert(runtime::protocol::message_transport_allowed(
        runtime::protocol::default_transports_for_domain(
            runtime::protocol::MessageDomain::kPublicRealtime),
        runtime::protocol::kMessageTransportInternal));
    assert(runtime::protocol::message_mode_allowed_for_domain(
        gate_login->domain,
        gate_login->interaction_mode));

    const auto* move = catalog.find_by_type("mmo.cs.MoveCommand");
    assert(move != nullptr);
    assert(move->message_id == 20000);
    assert(move->interaction_mode == runtime::protocol::MessageMode::kCast);
    assert(runtime::protocol::message_mode_allowed_for_domain(
        move->domain,
        move->interaction_mode));

    assert(!runtime::protocol::message_mode_allowed_for_domain(
        runtime::protocol::MessageDomain::kPublicRealtime,
        runtime::protocol::MessageMode::kReply));

    bool duplicate_rejected = false;
    try {
        catalog.add({
            10010,
            "mmo.cs.Duplicate",
            runtime::protocol::MessageDomain::kPublicReliable,
        });
    } catch (const std::runtime_error&) {
        duplicate_rejected = true;
    }
    assert(duplicate_rejected);

    bool wrong_domain_range_rejected = false;
    try {
        catalog.add({
            20001,
            "mmo.cs.WrongRange",
            runtime::protocol::MessageDomain::kPublicReliable,
        });
    } catch (const std::runtime_error&) {
        wrong_domain_range_rejected = true;
    }
    assert(wrong_domain_range_rejected);
}

void verify_reliable_frame_codec() {
    runtime::net::ReliableFrame frame;
    frame.version = 1;
    frame.message_id = 10010;
    frame.flags = 0x04;
    frame.request_id = 998877;
    frame.session_id = 11223344;
    frame.payload = {'g', 'a', 't', 'e'};

    std::string error;
    const auto encoded =
        runtime::net::ReliableFrameCodec::encode(frame, 1024, &error);
    assert(!encoded.empty());
    assert(error.empty());

    runtime::net::ReliableFrame decoded;
    assert(runtime::net::ReliableFrameCodec::decode(
        encoded, 1024, &decoded, &error));
    assert(decoded.version == frame.version);
    assert(decoded.message_id == frame.message_id);
    assert(decoded.flags == frame.flags);
    assert(decoded.request_id == frame.request_id);
    assert(decoded.session_id == frame.session_id);
    assert(decoded.payload == frame.payload);

    std::vector<std::uint8_t> truncated(encoded.begin(), encoded.end() - 1);
    assert(!runtime::net::ReliableFrameCodec::decode(
        truncated, 1024, &decoded, &error));
    assert(!error.empty());

    assert(!runtime::net::ReliableFrameCodec::decode(
        encoded, 2, &decoded, &error));
}

void verify_realtime_packet_codec() {
    runtime::net::RealtimePacket packet;
    packet.version = 1;
    packet.sequence = 777;
    packet.flags = 0x02;
    packet.message_id = 20000;
    packet.realtime_session_id = 9988;
    packet.payload = {'m', 'o', 'v', 'e'};

    std::string error;
    const auto encoded =
        runtime::net::RealtimePacketCodec::encode(packet, 1024, &error);
    assert(!encoded.empty());
    assert(error.empty());

    runtime::net::RealtimePacket decoded;
    assert(runtime::net::RealtimePacketCodec::decode(
        encoded, 1024, &decoded, &error));
    assert(decoded.version == packet.version);
    assert(decoded.sequence == packet.sequence);
    assert(decoded.flags == packet.flags);
    assert(decoded.message_id == packet.message_id);
    assert(decoded.realtime_session_id == packet.realtime_session_id);
    assert(decoded.payload == packet.payload);

    std::vector<std::uint8_t> truncated(encoded.begin(), encoded.end() - 1);
    assert(!runtime::net::RealtimePacketCodec::decode(
        truncated, 1024, &decoded, &error));
    assert(!error.empty());

    assert(!runtime::net::RealtimePacketCodec::decode(
        encoded, 2, &decoded, &error));
}

}  // namespace

int main() {
    verify_message_catalog();
    verify_reliable_frame_codec();
    verify_realtime_packet_codec();

    std::cout << "protocol codec governance probe ok\n";
    return 0;
}
