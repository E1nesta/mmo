#pragma once

#include <cstdint>
#include <string>

#include "common/envelope.pb.h"

namespace mmo::runtime::protocol {

std::int64_t current_time_millis();

bool sign_internal_envelope(
    mmo::common::Envelope* envelope,
    const std::string& source_service,
    std::int64_t timestamp_millis,
    const std::string& shared_secret,
    std::string* error_message);

bool validate_internal_envelope(
    const mmo::common::Envelope& envelope,
    const std::string& shared_secret,
    std::int64_t max_clock_skew_millis,
    std::int64_t now_millis,
    std::string* error_message);

bool sign_internal_envelope_now(
    mmo::common::Envelope* envelope,
    const std::string& source_service,
    const std::string& shared_secret,
    std::string* error_message);

bool validate_internal_envelope_now(
    const mmo::common::Envelope& envelope,
    const std::string& shared_secret,
    std::int64_t max_clock_skew_millis,
    std::string* error_message);

}  // namespace mmo::runtime::protocol
