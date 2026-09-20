#pragma once
#include <vita/codec/samples.hpp>
#include <vita/runtime/state/contracts.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace vita::profiles::iq {
enum class SampleFormat : std::uint8_t { iq16, iq32, float32 };

constexpr std::size_t bytes_per_pair(SampleFormat format) noexcept {
    switch (format) {
    case SampleFormat::iq16: return 4;
    case SampleFormat::iq32: case SampleFormat::float32: return 8;
    }
    return 0;
}
constexpr std::uint16_t baseline_packet_class(SampleFormat format) noexcept {
    return bytes_per_pair(format) ? static_cast<std::uint16_t>(format) + 1 : 0;
}
constexpr PayloadFormat payload_format(SampleFormat format) noexcept {
    switch (format) {
    case SampleFormat::iq16: return PayloadFormat{0x200003cf00000000ULL};
    case SampleFormat::iq32: return PayloadFormat{0x200007df00000000ULL};
    case SampleFormat::float32: return PayloadFormat{0x2e0007df00000000ULL};
    }
    return PayloadFormat{};
}

namespace detail {
// Power-of-two scaling is exact for binary floating point. Explicit tie handling
// is independent of the host's integer conversion rounding mode.
template<class Integer>
Integer normalized_integer(double value) noexcept {
    constexpr double scale = sizeof(Integer) == 2 ? 32768.0 : 2147483648.0;
    if (value <= -1.0) return std::numeric_limits<Integer>::min();
    if (value >= 1.0) return std::numeric_limits<Integer>::max();
    const double magnitude = std::abs(value) * scale;
    const auto integral = static_cast<std::uint64_t>(std::floor(magnitude));
    const double fraction = magnitude - static_cast<double>(integral);
    const auto rounded = integral + (fraction > 0.5 || (fraction == 0.5 && (integral & 1)));
    const auto signed_value = value < 0 ? -static_cast<std::int64_t>(rounded) : static_cast<std::int64_t>(rounded);
    if (signed_value > std::numeric_limits<Integer>::max()) return std::numeric_limits<Integer>::max();
    return static_cast<Integer>(signed_value);
}
} // namespace detail

inline Result<void> validate_samples(SampleFormat format, Bytes bytes) noexcept {
    const auto width = bytes_per_pair(format);
    if (!width || bytes.empty() || bytes.size() % width)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (format == SampleFormat::float32) {
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
            const auto value = std::bit_cast<float>(codec::detail::load32(bytes, offset));
            if (!std::isfinite(value))
                return std::unexpected(Error{ErrorCode::invalid_argument, offset});
        }
    }
    return {};
}

class SampleWriteWindow {
    MutableBytes wire_;
    SampleFormat format_;
    std::uint64_t first_ordinal_;
    std::size_t count_;
    const runtime::StateSnapshot& config_;
    std::array<std::uint64_t, 16> written_{};

    SampleWriteWindow(MutableBytes wire, SampleFormat format, std::uint64_t ordinal,
                      std::size_t count, const runtime::StateSnapshot& config) noexcept
        : wire_(wire), format_(format), first_ordinal_(ordinal), count_(count), config_(config) {}
public:
    SampleWriteWindow(const SampleWriteWindow&) = delete;
    SampleWriteWindow& operator=(const SampleWriteWindow&) = delete;
    SampleWriteWindow(SampleWriteWindow&&) noexcept = default;

    static Result<SampleWriteWindow> create(MutableBytes wire, SampleFormat format,
                                            std::uint64_t first_ordinal, std::size_t count,
                                            const runtime::StateSnapshot& config) noexcept {
        const auto width = bytes_per_pair(format);
        if (!width || !count || count > 1024)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        if (count - 1 > UINT64_MAX - first_ordinal)
            return std::unexpected(Error{ErrorCode::overflow});
        if (wire.size() != count * width)
            return std::unexpected(Error{ErrorCode::invalid_argument, 0, count * width});
        return SampleWriteWindow(wire, format, first_ordinal, count, config);
    }
    MutableBytes wire() const noexcept { return wire_; }
    SampleFormat format() const noexcept { return format_; }
    std::uint64_t first_ordinal() const noexcept { return first_ordinal_; }
    std::size_t count() const noexcept { return count_; }
    const runtime::StateSnapshot& config() const noexcept { return config_; }

    // Preserve device CS16 codes without a floating-point normalization step.
    Result<void> write_iq16(std::size_t index,std::int16_t i,std::int16_t q) noexcept {
        if(format_!=SampleFormat::iq16||index>=count_)
            return std::unexpected(Error{ErrorCode::invalid_argument,index});
        codec::detail::store32(wire_,index*4,(std::uint32_t{std::bit_cast<std::uint16_t>(i)}<<16)|std::bit_cast<std::uint16_t>(q));
        written_[index/64]|=std::uint64_t{1}<<(index%64);
        return {};
    }

    Result<void> write(std::size_t index, double i, double q) noexcept {
        if (index >= count_ || !std::isfinite(i) || !std::isfinite(q))
            return std::unexpected(Error{ErrorCode::invalid_argument, index});
        const auto offset = index * bytes_per_pair(format_);
        if (format_ == SampleFormat::iq16) {
            const auto real = std::bit_cast<std::uint16_t>(detail::normalized_integer<std::int16_t>(i));
            const auto imaginary = std::bit_cast<std::uint16_t>(detail::normalized_integer<std::int16_t>(q));
            codec::detail::store32(wire_, offset, (std::uint32_t{real} << 16) | imaginary);
        } else if (format_ == SampleFormat::iq32) {
            codec::detail::store32(wire_, offset, std::bit_cast<std::uint32_t>(detail::normalized_integer<std::int32_t>(i)));
            codec::detail::store32(wire_, offset + 4, std::bit_cast<std::uint32_t>(detail::normalized_integer<std::int32_t>(q)));
        } else {
            if (std::abs(i) > std::numeric_limits<float>::max() || std::abs(q) > std::numeric_limits<float>::max())
                return std::unexpected(Error{ErrorCode::overflow, index});
            codec::detail::store32(wire_, offset, std::bit_cast<std::uint32_t>(static_cast<float>(i)));
            codec::detail::store32(wire_, offset + 4, std::bit_cast<std::uint32_t>(static_cast<float>(q)));
        }
        written_[index / 64] |= std::uint64_t{1} << (index % 64);
        return {};
    }
    // A provider declares that the entire wire extent is initialized, either by
    // a bulk write or by reuse of previously initialized bytes. Validate semantic
    // wire values before marking coverage; failure leaves coverage unchanged.
    Result<void> complete_from_wire() noexcept {
        auto valid = validate_samples(format_, wire_);
        if (!valid) return valid;
        for (std::size_t i = 0; i < count_; ++i)
            written_[i / 64] |= std::uint64_t{1} << (i % 64);
        return {};
    }
    Result<void> validate_complete() const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (!(written_[i / 64] & (std::uint64_t{1} << (i % 64))))
                return std::unexpected(Error{ErrorCode::invalid_state, i});
        }
        return validate_samples(format_, wire_);
    }
};

struct SourceProvider {
    void* context = nullptr;
    Result<void> (*callback)(void*, SampleWriteWindow&) noexcept = nullptr;
    Result<void> (*effective)(void*,const runtime::EffectiveEvent&) noexcept = nullptr;
    Result<void> produce(SampleWriteWindow& window) const noexcept {
        if (!callback) return std::unexpected(Error{ErrorCode::invalid_state});
        return callback(context, window);
    }
};

// Binary64 constants for amplitude 0.5*cos(2*pi*n/16), with exact axis zeros.
// Q uses the quadrature entry, yielding positive complex frequency fs/16.
inline constexpr std::array<double, 16> canonical_cosine{
    0.5, 0.461939766255643378, 0.353553390593273762, 0.191341716182544885,
    0.0, -0.191341716182544885, -0.353553390593273762, -0.461939766255643378,
    -0.5, -0.461939766255643378, -0.353553390593273762, -0.191341716182544885,
    0.0, 0.191341716182544885, 0.353553390593273762, 0.461939766255643378
};
inline constexpr auto canonical_float_cosine = [] {
    std::array<float, 16> values{};
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = static_cast<float>(canonical_cosine[i]);
    return values;
}();
inline SourceProvider default_source() noexcept {
    return {nullptr, [](void*, SampleWriteWindow& window) noexcept -> Result<void> {
        for (std::size_t i = 0; i < window.count(); ++i) {
            const auto phase = static_cast<std::size_t>((window.first_ordinal() % 16 + i) % 16);
            const auto real = window.format() == SampleFormat::float32 ? static_cast<double>(canonical_float_cosine[phase]) : canonical_cosine[phase];
            const auto imaginary = window.format() == SampleFormat::float32 ? static_cast<double>(canonical_float_cosine[(phase + 12) % 16]) : canonical_cosine[(phase + 12) % 16];
            auto written = window.write(i, real, imaginary);
            if (!written) return written;
        }
        return window.validate_complete();
    }};
}
} // namespace vita::profiles::iq
