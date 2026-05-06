#include "runtime/protocol/message_catalog.h"

#include <stdexcept>
#include <utility>

namespace runtime::protocol {

namespace {

bool message_id_matches_domain(std::uint32_t message_id, MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kPublicReliable:
            return message_id >= 1U && message_id <= 9999U;
        case MessageDomain::kPublicRealtime:
            return message_id >= 10000U && message_id <= 19999U;
        case MessageDomain::kGatewayInternal:
            return message_id >= 20000U && message_id <= 39999U;
        case MessageDomain::kBackendInternal:
            return message_id >= 40000U && message_id <= 59999U;
        case MessageDomain::kSystem:
            return message_id >= 60000U && message_id <= 69999U;
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
    if (!message_id_matches_domain(descriptor.message_id, descriptor.domain)) {
        throw std::runtime_error("message id does not match message domain range");
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

const char* message_domain_name(MessageDomain domain) {
    switch (domain) {
        case MessageDomain::kPublicReliable:
            return "public_reliable";
        case MessageDomain::kPublicRealtime:
            return "public_realtime";
        case MessageDomain::kGatewayInternal:
            return "gateway_internal";
        case MessageDomain::kBackendInternal:
            return "backend_internal";
        case MessageDomain::kSystem:
            return "system";
    }
    return "unknown";
}

}  // namespace runtime::protocol
