#pragma once
#include <vita/core/error.hpp>
#include <bit>
#include <cstdint>
#include <limits>

namespace vita::codec::numeric {
// Exact finite dimensionless value; noncanonical input is accepted by encode.
struct BinaryValue {
    std::uint64_t magnitude;
    std::int16_t exponent;
    bool negative;
    friend constexpr bool operator==(BinaryValue, BinaryValue) = default;
};
enum class Kind { unsigned_fixed, signed_fixed, unsigned_non_normalized,
                  signed_non_normalized, unsigned_vrt, signed_vrt };
struct NumericSpec {
    Kind kind;
    unsigned bits;
    unsigned fractional_bits = 0;
    unsigned exponent_bits = 0;
};
enum class Precision { exact_only, allow_rounding };
enum class Rounding { nearest_ties_even, toward_zero, toward_negative, toward_positive };
enum class Overflow { error, saturate };
enum class VrtEncoding { lowest_exponent };
struct ConversionPolicy {
    Precision precision;
    Rounding rounding;
    Overflow overflow;
    VrtEncoding vrt_encoding;
    ConversionPolicy() = delete;
    constexpr ConversionPolicy(Precision p, Rounding r, Overflow o, VrtEncoding v) noexcept
        : precision(p), rounding(r), overflow(o), vrt_encoding(v) {}
};
struct EncodedValue {
    std::uint64_t bits;
    bool rounded;
    bool saturated;
};
namespace detail {
constexpr std::uint64_t mask(unsigned n) noexcept {
    return n == 64 ? UINT64_MAX : (std::uint64_t{1} << n) - 1;
}
constexpr bool signed_kind(Kind k) noexcept {
    return k == Kind::signed_fixed || k == Kind::signed_non_normalized || k == Kind::signed_vrt;
}
constexpr bool vrt(Kind k) noexcept { return k == Kind::signed_vrt || k == Kind::unsigned_vrt; }
constexpr bool non_normalized(Kind k) noexcept {
    return k == Kind::signed_non_normalized || k == Kind::unsigned_non_normalized;
}
constexpr Result<void> validate(NumericSpec s) noexcept {
    if (s.kind < Kind::unsigned_fixed || s.kind > Kind::signed_vrt || s.bits < 1 || s.bits > 64)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (vrt(s.kind)) {
        if (s.exponent_bits < 1 || s.exponent_bits > 6 || s.exponent_bits >= s.bits || s.fractional_bits)
            return std::unexpected(Error{ErrorCode::invalid_argument});
    } else if (s.exponent_bits || (non_normalized(s.kind)
               ? (s.fractional_bits >= s.bits || s.fractional_bits > 15) : s.fractional_bits != 0)) {
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    return {};
}
constexpr bool valid(ConversionPolicy p) noexcept {
    return (p.precision == Precision::exact_only || p.precision == Precision::allow_rounding)
        && p.rounding >= Rounding::nearest_ties_even && p.rounding <= Rounding::toward_positive
        && (p.overflow == Overflow::error || p.overflow == Overflow::saturate)
        && p.vrt_encoding == VrtEncoding::lowest_exponent;
}
constexpr unsigned mantissa_bits(NumericSpec s) noexcept { return s.bits - (vrt(s.kind) ? s.exponent_bits : 0); }
constexpr int quantum(NumericSpec s, unsigned e) noexcept {
    if (non_normalized(s.kind)) return -static_cast<int>(s.fractional_bits);
    const int width = static_cast<int>(mantissa_bits(s)) - (signed_kind(s.kind) ? 1 : 0);
    return -width + (vrt(s.kind) ? static_cast<int>(e) - static_cast<int>(mask(s.exponent_bits)) : 0);
}
// Compare positive magnitudes without shifting out bits, including extreme input exponents.
constexpr int compare(BinaryValue a, BinaryValue b) noexcept {
    if (!a.magnitude || !b.magnitude) return a.magnitude ? 1 : b.magnitude ? -1 : 0;
    const unsigned aw = std::bit_width(a.magnitude), bw = std::bit_width(b.magnitude);
    const int atop = static_cast<int>(a.exponent) + static_cast<int>(aw);
    const int btop = static_cast<int>(b.exponent) + static_cast<int>(bw);
    if (atop != btop) return atop < btop ? -1 : 1;
    const auto am = a.magnitude << (64 - aw), bm = b.magnitude << (64 - bw);
    return am < bm ? -1 : am > bm ? 1 : 0;
}
struct Quantized { std::uint64_t magnitude; bool residue; bool overflow; };
constexpr Quantized quantize(BinaryValue value, int q, Rounding rounding) noexcept {
    const int shift = static_cast<int>(value.exponent) - q;
    if (!value.magnitude) return {0, false, false};
    if (shift >= 0) {
        if (shift >= 64 || static_cast<int>(std::bit_width(value.magnitude)) + shift > 64)
            return {0, false, true};
        return {value.magnitude << shift, false, false};
    }
    const unsigned right = static_cast<unsigned>(-shift);
    std::uint64_t quotient = right >= 64 ? 0 : value.magnitude >> right;
    const std::uint64_t remainder = right >= 64 ? value.magnitude : value.magnitude & mask(right);
    bool increment = false;
    if (remainder) {
        if (rounding == Rounding::toward_negative) increment = value.negative;
        if (rounding == Rounding::toward_positive) increment = !value.negative;
        if (rounding == Rounding::nearest_ties_even && right <= 64) {
            const auto half = std::uint64_t{1} << (right - 1);
            increment = remainder > half || (remainder == half && (quotient & 1));
        }
    }
    if (increment && quotient == UINT64_MAX) return {0, remainder != 0, true};
    return {quotient + static_cast<unsigned>(increment), remainder != 0, false};
}
} // namespace detail

constexpr Result<BinaryValue> canonicalize(BinaryValue value) noexcept {
    if (!value.magnitude) return BinaryValue{0, 0, false};
    const int trailing = std::countr_zero(value.magnitude);
    const int exponent = static_cast<int>(value.exponent) + trailing;
    if (exponent > std::numeric_limits<std::int16_t>::max())
        return std::unexpected(Error{ErrorCode::overflow});
    return BinaryValue{value.magnitude >> trailing, static_cast<std::int16_t>(exponent), value.negative};
}
constexpr Result<BinaryValue> decode_exact(NumericSpec spec, std::uint64_t bits) noexcept {
    auto valid = detail::validate(spec);
    if (!valid) return std::unexpected(valid.error());
    if (bits & ~detail::mask(spec.bits)) return std::unexpected(Error{ErrorCode::invalid_argument});
    const unsigned m = detail::mantissa_bits(spec);
    const unsigned e = detail::vrt(spec.kind) ? static_cast<unsigned>(bits & detail::mask(spec.exponent_bits)) : 0;
    const auto mantissa = detail::vrt(spec.kind) ? bits >> spec.exponent_bits : bits;
    const bool negative = detail::signed_kind(spec.kind) && (mantissa & (std::uint64_t{1} << (m - 1)));
    const auto magnitude = negative ? ((~mantissa + 1) & detail::mask(m)) : mantissa;
    return canonicalize({magnitude, static_cast<std::int16_t>(detail::quantum(spec, e)), negative});
}
// Domain overflow is checked before quantization, even when rounding could bring
// the input back into range. exact_only rejects both quantization and clipping.
constexpr Result<EncodedValue> encode_numeric(NumericSpec spec, BinaryValue value, ConversionPolicy policy) noexcept {
    auto valid = detail::validate(spec);
    if (!valid) return std::unexpected(valid.error());
    if (!detail::valid(policy)) return std::unexpected(Error{ErrorCode::invalid_argument});
    if (!value.magnitude) return EncodedValue{0, false, false};
    const bool signed_format = detail::signed_kind(spec.kind);
    const unsigned m = detail::mantissa_bits(spec);
    const unsigned max_e = detail::vrt(spec.kind) ? static_cast<unsigned>(detail::mask(spec.exponent_bits)) : 0;
    const std::uint64_t max_mag = signed_format
        ? (value.negative ? std::uint64_t{1} << (m - 1) : detail::mask(m - 1)) : detail::mask(m);
    BinaryValue bound{max_mag, static_cast<std::int16_t>(detail::quantum(spec, max_e)), value.negative};
    bool saturated = false;
    if ((!signed_format && value.negative) || detail::compare(value, bound) > 0) {
        if (policy.overflow == Overflow::error || policy.precision == Precision::exact_only)
            return std::unexpected(Error{ErrorCode::overflow});
        saturated = true;
        if (!signed_format && value.negative) return EncodedValue{0, false, true};
        value = bound;
    }
    for (unsigned e = 0; e <= max_e; ++e) {
        const auto result = detail::quantize(value, detail::quantum(spec, e), policy.rounding);
        if (result.overflow || result.magnitude > max_mag) continue;
        if (result.residue && policy.precision == Precision::exact_only) continue;
        auto magnitude = result.magnitude;
        unsigned encoded_e = e;
        // Rounding at a coarser quantum can land back inside the finer domain.
        // Canonicalize that final value without rounding a second time.
        while (encoded_e && magnitude && magnitude <= max_mag / 2) {
            magnitude *= 2;
            --encoded_e;
        }
        const auto mantissa = value.negative ? ((~magnitude + 1) & detail::mask(m)) : magnitude;
        // Zero always uses its canonical all-zero representation.
        const auto bits = !result.magnitude ? 0 : detail::vrt(spec.kind)
            ? (mantissa << spec.exponent_bits) | encoded_e : mantissa;
        return EncodedValue{bits, result.residue, saturated};
    }
    // Input was within the global domain; only a precision policy can prevent a result.
    return std::unexpected(Error{ErrorCode::invalid_argument});
}
constexpr Result<EncodedValue> convert_numeric(NumericSpec source, std::uint64_t bits,
                                               NumericSpec destination, ConversionPolicy policy) noexcept {
    auto value = decode_exact(source, bits);
    if (!value) return std::unexpected(value.error());
    return encode_numeric(destination, *value, policy);
}
} // namespace vita::codec::numeric
