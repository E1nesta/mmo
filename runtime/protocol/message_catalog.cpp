#include "runtime/protocol/message_catalog.h"

#include <stdexcept>
#include <utility>

namespace runtime::protocol {

namespace {

bool message_id_matches_domain(std::uint32_t message_id, MessageDomain domain) {
    const auto range = message_domain_range(domain);
    return message_id >= range.first && message_id <= range.last;
}

bool transports_match_domain(
    MessageTransportMask allowed_transports,
    MessageDomain domain) {
    const auto default_transports = default_transports_for_domain(domain);
    return allowed_transports != 0U &&
           (allowed_transports & ~default_transports) == 0U;
}

bool mode_matches_domain(MessageMode mode, MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kSystem:
        case MessageDomain::kBackendInternal:
        case MessageDomain::kGatewayInternal:
            return is_valid_message_mode(mode);
        case MessageDomain::kPublicReliable:
            return mode == MessageMode::kCall ||
                   mode == MessageMode::kReply ||
                   mode == MessageMode::kCast ||
                   mode == MessageMode::kBatch;
        case MessageDomain::kPublicRealtime:
            return mode == MessageMode::kCast || mode == MessageMode::kBatch;
    }
    return false;
}

}  // namespace

void MessageCatalog::add(MessageDescriptor descriptor) {
    if (descriptor.message_id == 0U) {
        throw std::runtime_error("message id must be non-zero");
    }
    if (descriptor.message_type.empty()) {
        throw std::runtime_error("message type must be non-empty");
    }
    if (descriptor.allowed_transports == 0U) {
        descriptor.allowed_transports =
            default_transports_for_domain(descriptor.domain);
    }
    if (!message_id_matches_domain(descriptor.message_id, descriptor.domain)) {
        throw std::runtime_error("message id does not match message domain range");
    }
    if (!transports_match_domain(
            descriptor.allowed_transports,
            descriptor.domain)) {
        throw std::runtime_error(
            "message transport does not match message domain");
    }
    if (!mode_matches_domain(
            descriptor.interaction_mode,
            descriptor.domain)) {
        throw std::runtime_error("message mode does not match message domain");
    }
    if (by_id_.find(descriptor.message_id) != by_id_.end()) {
        throw std::runtime_error("duplicate message id");
    }
    if (ids_by_type_.find(descriptor.message_type) != ids_by_type_.end()) {
        throw std::runtime_error("duplicate message type");
    }

    const auto message_id = descriptor.message_id;
    const auto message_type = descriptor.message_type;
    by_id_.emplace(message_id, std::move(descriptor));
    ids_by_type_.emplace(message_type, message_id);
}

const MessageDescriptor* MessageCatalog::find_by_id(
    std::uint32_t message_id) const {
    const auto it = by_id_.find(message_id);
    return it == by_id_.end() ? nullptr : &it->second;
}

const MessageDescriptor* MessageCatalog::find_by_type(
    const std::string& message_type) const {
    const auto id_it = ids_by_type_.find(message_type);
    if (id_it == ids_by_type_.end()) {
        return nullptr;
    }
    return find_by_id(id_it->second);
}

bool MessageCatalog::contains_id(std::uint32_t message_id) const {
    return find_by_id(message_id) != nullptr;
}

bool MessageCatalog::contains_type(const std::string& message_type) const {
    return find_by_type(message_type) != nullptr;
}

MessageIdRange message_domain_range(MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kSystem:
            return {1U, 9999U};
        case MessageDomain::kPublicReliable:
            return {10000U, 19999U};
        case MessageDomain::kPublicRealtime:
            return {20000U, 29999U};
        case MessageDomain::kBackendInternal:
            return {30000U, 49999U};
        case MessageDomain::kGatewayInternal:
            return {50000U, 59999U};
    }
    return {};
}

const char* message_domain_name(MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kSystem:
            return "system";
        case MessageDomain::kPublicReliable:
            return "public_reliable";
        case MessageDomain::kPublicRealtime:
            return "public_realtime";
        case MessageDomain::kBackendInternal:
            return "backend_internal";
        case MessageDomain::kGatewayInternal:
            return "gateway_internal";
    }
    return "unknown";
}

MessageTransportMask default_transports_for_domain(MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kSystem:
            return kMessageTransportTcp |
                   kMessageTransportWebSocket |
                   kMessageTransportKcp |
                   kMessageTransportInternal;
        case MessageDomain::kPublicReliable:
            return kMessageTransportTcp |
                   kMessageTransportWebSocket |
                   kMessageTransportInternal;
        case MessageDomain::kPublicRealtime:
            return kMessageTransportKcp | kMessageTransportInternal;
        case MessageDomain::kBackendInternal:
        case MessageDomain::kGatewayInternal:
            return kMessageTransportInternal;
    }
    return 0U;
}

bool message_mode_allowed_for_domain(MessageDomain domain, MessageMode mode) {
    return mode_matches_domain(mode, domain);
}

bool message_transport_allowed(
    MessageTransportMask allowed_transports,
    MessageTransportMask transport) {
    return transport != 0U && (allowed_transports & transport) == transport;
}

}  // namespace runtime::protocol
