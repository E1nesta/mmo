#include "runtime/transport/kcp_session.h"

#include <utility>

namespace mmo::runtime::transport {

KcpSession::KcpSession(std::uint32_t conversation_id, KcpOutput output, KcpOptions options)
    : conversation_id_(conversation_id), output_(std::move(output)) {
    kcp_ = ikcp_create(conversation_id_, this);
    ikcp_setoutput(kcp_, &KcpSession::output_callback);
    ikcp_nodelay(kcp_,
                 options.nodelay,
                 options.interval_millis,
                 options.fast_resend,
                 options.disable_congestion_control);
    ikcp_wndsize(kcp_, options.send_window, options.receive_window);
}

KcpSession::~KcpSession() {
    if (kcp_ != nullptr) {
        ikcp_release(kcp_);
    }
}

KcpSession::KcpSession(KcpSession&& other) noexcept
    : conversation_id_(other.conversation_id_),
      output_(std::move(other.output_)),
      kcp_(other.kcp_) {
    other.kcp_ = nullptr;
    if (kcp_ != nullptr) {
        kcp_->user = this;
    }
}

KcpSession& KcpSession::operator=(KcpSession&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (kcp_ != nullptr) {
        ikcp_release(kcp_);
    }
    conversation_id_ = other.conversation_id_;
    output_ = std::move(other.output_);
    kcp_ = other.kcp_;
    other.kcp_ = nullptr;
    if (kcp_ != nullptr) {
        kcp_->user = this;
    }
    return *this;
}

std::uint32_t KcpSession::conversation_id() const {
    return conversation_id_;
}

int KcpSession::send_reliable(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        return 0;
    }
    const auto* data = reinterpret_cast<const char*>(payload.data());
    return ikcp_send(kcp_, data, static_cast<int>(payload.size()));
}

int KcpSession::input_packet(const char* data, int size) {
    if (data == nullptr || size <= 0) {
        return -1;
    }
    return ikcp_input(kcp_, data, size);
}

void KcpSession::update(std::uint32_t current_millis) {
    ikcp_update(kcp_, current_millis);
}

std::uint32_t KcpSession::next_update_millis(std::uint32_t current_millis) const {
    return ikcp_check(kcp_, current_millis);
}

int KcpSession::output_callback(const char* data, int size, ikcpcb* /*kcp*/, void* user) {
    auto* session = static_cast<KcpSession*>(user);
    if (session == nullptr || !session->output_) {
        return -1;
    }
    return session->output_(data, size);
}

}  // namespace mmo::runtime::transport
