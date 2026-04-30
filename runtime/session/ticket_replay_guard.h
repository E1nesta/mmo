#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mmo::runtime::session {

class TicketReplayStore {
public:
    virtual ~TicketReplayStore() = default;
    virtual bool consume(
        const std::string& ticket_id,
        std::uint64_t now_millis,
        std::uint64_t expire_at_millis) = 0;
};

class TicketReplayGuard final : public TicketReplayStore {
public:
    bool consume(
        const std::string& ticket_id,
        std::uint64_t now_millis,
        std::uint64_t expire_at_millis) override;
    void purge_expired(std::uint64_t now_millis);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::uint64_t> consumed_ticket_ids_;
};

}  // namespace mmo::runtime::session
