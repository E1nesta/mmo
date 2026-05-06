#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace runtime::protocol {

enum class MessageDomain {
    kPublicReliable = 1,
    kPublicRealtime = 2,
    kGatewayInternal = 3,
    kBackendInternal = 4,
    kSystem = 5,
};

struct MessageDescriptor {
    std::uint32_t message_id{};
    std::string message_type;
    MessageDomain domain{MessageDomain::kPublicReliable};
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

const char* message_domain_name(MessageDomain domain);

}  // namespace runtime::protocol
