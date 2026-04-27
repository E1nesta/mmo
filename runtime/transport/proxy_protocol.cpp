// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/transport/proxy_protocol.h"

#include <boost/asio/ip/address.hpp>

#include <charconv>
#include <string_view>

namespace framework::transport {

namespace {

std::string FormatPeerAddress(const boost::asio::ip::address& address, std::uint16_t port) {
    const auto rendered = address.to_string();
    if (address.is_v6()) {
        return "[" + rendered + "]:" + std::to_string(port);
    }
    return rendered + ":" + std::to_string(port);
}

bool ParsePort(std::string_view raw_port, std::uint16_t* port, std::string* error_message) {
    unsigned int parsed = 0;
    const auto* begin = raw_port.data();
    const auto* end = raw_port.data() + raw_port.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed > 65535U) {
        if (error_message != nullptr) {
            *error_message = "invalid proxy protocol source port";
        }
        return false;
    }
    if (port != nullptr) {
        *port = static_cast<std::uint16_t>(parsed);
    }
    return true;
}

}  // namespace

bool ParseProxyProtocolHeader(std::string_view header_line,
                              std::string* peer_address,
                              std::string* error_message) {
    if (peer_address == nullptr) {
        if (error_message != nullptr) {
            *error_message = "peer_address output is null";
        }
        return false;
    }

    if (header_line.size() < 2 || header_line.substr(header_line.size() - 2) != "\r\n") {
        if (error_message != nullptr) {
            *error_message = "proxy protocol line must end with CRLF";
        }
        return false;
    }
    if (header_line.size() > kMaxProxyProtocolHeaderBytes) {
        if (error_message != nullptr) {
            *error_message = "proxy protocol line exceeds max size";
        }
        return false;
    }

    auto line = header_line.substr(0, header_line.size() - 2);
    const auto first_space = line.find(' ');
    if (first_space == std::string_view::npos || line.substr(0, first_space) != "PROXY") {
        if (error_message != nullptr) {
            *error_message = "invalid proxy protocol signature";
        }
        return false;
    }

    line.remove_prefix(first_space + 1);
    const auto protocol_end = line.find(' ');
    const auto protocol = protocol_end == std::string_view::npos ? line : line.substr(0, protocol_end);
    if (protocol == "UNKNOWN") {
        peer_address->clear();
        return true;
    }

    if (protocol != "TCP4" && protocol != "TCP6") {
        if (error_message != nullptr) {
            *error_message = "unsupported proxy protocol transport";
        }
        return false;
    }
    if (protocol_end == std::string_view::npos) {
        if (error_message != nullptr) {
            *error_message = "missing proxy protocol addresses";
        }
        return false;
    }

    line.remove_prefix(protocol_end + 1);
    const auto src_end = line.find(' ');
    if (src_end == std::string_view::npos) {
        if (error_message != nullptr) {
            *error_message = "missing proxy protocol source address";
        }
        return false;
    }
    const auto source_address = line.substr(0, src_end);
    line.remove_prefix(src_end + 1);

    const auto dst_end = line.find(' ');
    if (dst_end == std::string_view::npos) {
        if (error_message != nullptr) {
            *error_message = "missing proxy protocol destination address";
        }
        return false;
    }
    line.remove_prefix(dst_end + 1);

    const auto src_port_end = line.find(' ');
    if (src_port_end == std::string_view::npos) {
        if (error_message != nullptr) {
            *error_message = "missing proxy protocol source port";
        }
        return false;
    }
    const auto source_port_text = line.substr(0, src_port_end);

    std::uint16_t source_port = 0;
    if (!ParsePort(source_port_text, &source_port, error_message)) {
        return false;
    }

    boost::system::error_code address_error;
    const auto address = boost::asio::ip::make_address(std::string(source_address), address_error);
    if (address_error) {
        if (error_message != nullptr) {
            *error_message = "invalid proxy protocol source address";
        }
        return false;
    }
    if ((protocol == "TCP4" && !address.is_v4()) || (protocol == "TCP6" && !address.is_v6())) {
        if (error_message != nullptr) {
            *error_message = "proxy protocol transport/address family mismatch";
        }
        return false;
    }

    *peer_address = FormatPeerAddress(address, source_port);
    return true;
}

}  // namespace framework::transport
