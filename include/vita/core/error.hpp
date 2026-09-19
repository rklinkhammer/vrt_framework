#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
namespace vita {
enum class ErrorCode { invalid_argument, unsupported_layout, unsupported_capability,
    overflow, short_input, short_output, capacity_exhausted, stale_generation,
    invalid_state, would_deadlock, callback_failure, no_cpu_access, resource_limit, identity_conflict };
enum class EffectState { none, known, unknown };
enum class ErrorStage { none, validation, encoding, decoding, admission, execution, transport, lifecycle };
struct Error {
    ErrorCode code;
    std::size_t offset = 0;
    std::size_t required_capacity = 0;
    std::size_t consumed = 0;
    std::uint16_t field_id = 0;
    bool retryable = false;
    EffectState effect = EffectState::none;
    ErrorStage stage = ErrorStage::none;
    std::int32_t native_error = 0; // Native API status; offset remains a byte position.
    friend constexpr bool operator==(const Error&, const Error&) = default;
};
template<class T> using Result = std::expected<T, Error>;
} // namespace vita
