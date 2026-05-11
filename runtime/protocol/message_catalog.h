#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "runtime/protocol/message_mode.h"

namespace runtime::protocol {

enum class MessageDomain {
    kSystem = 1,
    kPublicReliable = 2,
    kPublicRealtime = 3,
    kBackendInternal = 4,
    kGatewayInternal = 5,
};

using MessageTransportMask = std::uint32_t;

inline constexpr MessageTransportMask kMessageTransportTcp = 1U << 0U;
inline constexpr MessageTransportMask kMessageTransportWebSocket = 1U << 1U;
inline constexpr MessageTransportMask kMessageTransportKcp = 1U << 2U;
inline constexpr MessageTransportMask kMessageTransportInternal = 1U << 3U;

struct MessageIdRange {
    std::uint32_t first{};
    std::uint32_t last{};
};

struct MessageDescriptor {
    std::uint32_t message_id{};
    std::string message_type;
    MessageDomain domain{MessageDomain::kPublicReliable};
    MessageTransportMask allowed_transports{};
    MessageMode interaction_mode{MessageMode::kCall};
};

class MessageCatalog {
public:
    void add(MessageDescriptor descriptor);

    const MessageDescriptor* find_by_id(std::uint32_t message_id) const;
    const MessageDescriptor* find_by_type(const std::string& message_type) const;
    bool contains_id(std::uint32_t message_id) const;
    bool contains_type(const std::string& message_type) const;

private:
    std::unordered_map<std::uint32_t, MessageDescriptor> by_id_;
    std::unordered_map<std::string, std::uint32_t> ids_by_type_;
};

MessageIdRange message_domain_range(MessageDomain domain);
const char* message_domain_name(MessageDomain domain);
MessageTransportMask default_transports_for_domain(MessageDomain domain);
bool message_mode_allowed_for_domain(MessageDomain domain, MessageMode mode);
bool message_transport_allowed(
    MessageTransportMask allowed_transports,
    MessageTransportMask transport);

}  // namespace runtime::protocol
