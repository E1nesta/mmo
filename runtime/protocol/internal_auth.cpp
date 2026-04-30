#include "runtime/protocol/internal_auth.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <chrono>
#include <cstdlib>
#include <string>

namespace mmo::runtime::protocol {
namespace {

std::string to_hex(const unsigned char* data, std::size_t size) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string output;
    output.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        output.push_back(kHexDigits[(data[index] >> 4U) & 0x0FU]);
        output.push_back(kHexDigits[data[index] & 0x0FU]);
    }
    return output;
}

void append_field(const std::string& field, std::string* output) {
    output->append(std::to_string(field.size()));
    output->push_back(':');
    output->append(field);
}

std::string canonical_payload(const mmo::common::Envelope& envelope) {
    std::string output;
    output.reserve(envelope.payload().size() + 256);
    append_field(envelope.source_service(), &output);
    append_field(std::to_string(envelope.request_id()), &output);
    append_field(envelope.message_type(), &output);
    append_field(std::to_string(envelope.player_id()), &output);
    append_field(envelope.session_token(), &output);
    append_field(envelope.game_session_id(), &output);
    append_field(envelope.trace_id(), &output);
    append_field(std::to_string(envelope.internal_timestamp_millis()), &output);
    append_field(envelope.payload(), &output);
    return output;
}

bool compute_hmac_sha256(
    const std::string& payload,
    const std::string& shared_secret,
    std::string* signature) {
    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(
            EVP_sha256(),
            shared_secret.data(),
            static_cast<int>(shared_secret.size()),
            reinterpret_cast<const unsigned char*>(payload.data()),
            payload.size(),
            digest,
            &digest_length) == nullptr) {
        return false;
    }
    *signature = to_hex(digest, digest_length);
    return true;
}

bool timing_safe_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

}  // namespace

std::int64_t current_time_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool sign_internal_envelope(
    mmo::common::Envelope* envelope,
    const std::string& source_service,
    std::int64_t timestamp_millis,
    const std::string& shared_secret,
    std::string* error_message) {
    if (envelope == nullptr) {
        if (error_message != nullptr) {
            *error_message = "envelope is null";
        }
        return false;
    }
    if (source_service.empty()) {
        if (error_message != nullptr) {
            *error_message = "source service is empty";
        }
        return false;
    }
    if (shared_secret.empty()) {
        if (error_message != nullptr) {
            *error_message = "internal auth shared secret is empty";
        }
        return false;
    }
    if (timestamp_millis <= 0) {
        if (error_message != nullptr) {
            *error_message = "internal auth timestamp is invalid";
        }
        return false;
    }

    envelope->set_source_service(source_service);
    envelope->set_internal_timestamp_millis(timestamp_millis);
    envelope->clear_internal_signature();

    std::string signature;
    if (!compute_hmac_sha256(canonical_payload(*envelope), shared_secret, &signature)) {
        if (error_message != nullptr) {
            *error_message = "failed to compute internal signature";
        }
        return false;
    }

    envelope->set_internal_signature(signature);
    return true;
}

bool validate_internal_envelope(
    const mmo::common::Envelope& envelope,
    const std::string& shared_secret,
    std::int64_t max_clock_skew_millis,
    std::int64_t now_millis,
    std::string* error_message) {
    if (shared_secret.empty()) {
        if (error_message != nullptr) {
            *error_message = "internal auth shared secret is empty";
        }
        return false;
    }
    if (envelope.source_service().empty()) {
        if (error_message != nullptr) {
            *error_message = "missing source service";
        }
        return false;
    }
    if (envelope.internal_timestamp_millis() <= 0) {
        if (error_message != nullptr) {
            *error_message = "missing internal auth timestamp";
        }
        return false;
    }
    if (max_clock_skew_millis <= 0) {
        if (error_message != nullptr) {
            *error_message = "internal auth clock skew must be greater than zero";
        }
        return false;
    }
    if (std::llabs(now_millis - envelope.internal_timestamp_millis()) >
        max_clock_skew_millis) {
        if (error_message != nullptr) {
            *error_message = "internal auth timestamp out of range";
        }
        return false;
    }
    if (envelope.internal_signature().empty()) {
        if (error_message != nullptr) {
            *error_message = "missing internal signature";
        }
        return false;
    }

    auto unsigned_envelope = envelope;
    const std::string signature = unsigned_envelope.internal_signature();
    unsigned_envelope.clear_internal_signature();

    std::string expected;
    if (!compute_hmac_sha256(
            canonical_payload(unsigned_envelope), shared_secret, &expected) ||
        !timing_safe_equal(signature, expected)) {
        if (error_message != nullptr) {
            *error_message = "invalid internal signature";
        }
        return false;
    }
    return true;
}

bool sign_internal_envelope_now(
    mmo::common::Envelope* envelope,
    const std::string& source_service,
    const std::string& shared_secret,
    std::string* error_message) {
    return sign_internal_envelope(
        envelope,
        source_service,
        current_time_millis(),
        shared_secret,
        error_message);
}

bool validate_internal_envelope_now(
    const mmo::common::Envelope& envelope,
    const std::string& shared_secret,
    std::int64_t max_clock_skew_millis,
    std::string* error_message) {
    return validate_internal_envelope(
        envelope,
        shared_secret,
        max_clock_skew_millis,
        current_time_millis(),
        error_message);
}

}  // namespace mmo::runtime::protocol
