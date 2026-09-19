#pragma once
#include <vita/codec/numeric_samples.hpp>
#include <bit>
#include <cstdint>

namespace vita::codec::ieee {
enum class Format { binary16, binary32, binary64 };
enum class Category { zero, finite, infinity, quiet_nan, signaling_nan };
enum class Overflow { error, ieee_result, saturate_finite };
enum class NonFinite { reject, propagate };
enum class NaNPayload { exact, preserve_high, canonical };
struct Policy {
    numeric::Precision precision;
    numeric::Rounding rounding;
    Overflow overflow;
    NonFinite nonfinite;
    NaNPayload payload;
    Policy() = delete;
    constexpr Policy(numeric::Precision p, numeric::Rounding r, Overflow o, NonFinite n, NaNPayload a)
        : precision(p), rounding(r), overflow(o), nonfinite(n), payload(a) {}
};
struct Flags { bool invalid=false, inexact=false, overflow=false, underflow=false, payload_loss=false; };
struct Converted { std::uint64_t bits; Flags flags; };
struct Decoded {
    Category category;
    bool negative;
    numeric::BinaryValue finite;
    std::uint64_t payload;
    unsigned payload_bits;
};
namespace detail {
struct Shape { unsigned width, fraction, exponent; int bias; };
constexpr Result<Shape> shape(Format f) noexcept {
    switch(f) {
        case Format::binary16: return Shape{16,10,5,15};
        case Format::binary32: return Shape{32,23,8,127};
        case Format::binary64: return Shape{64,52,11,1023};
    }
    return std::unexpected(Error{ErrorCode::invalid_argument});
}
constexpr std::uint64_t mask(unsigned n) noexcept { return n==64 ? UINT64_MAX : (std::uint64_t{1}<<n)-1; }
constexpr bool valid(Policy p) noexcept {
    return (p.precision==numeric::Precision::exact_only || p.precision==numeric::Precision::allow_rounding)
        && p.rounding>=numeric::Rounding::nearest_ties_even && p.rounding<=numeric::Rounding::toward_positive
        && p.overflow>=Overflow::error && p.overflow<=Overflow::saturate_finite
        && p.nonfinite>=NonFinite::reject && p.nonfinite<=NonFinite::propagate
        && p.payload>=NaNPayload::exact && p.payload<=NaNPayload::canonical;
}
struct Rounded { std::uint64_t integer; bool inexact; };
// The selected quantum leaves at most 53 significant output bits.
constexpr Rounded round(numeric::BinaryValue v, int quantum, numeric::Rounding mode) noexcept {
    int shift=static_cast<int>(v.exponent)-quantum;
    if(shift>=0) return {v.magnitude<<shift,false};
    unsigned right=static_cast<unsigned>(-shift);
    std::uint64_t integer=right>=64?0:v.magnitude>>right;
    std::uint64_t residue=right>=64?v.magnitude:v.magnitude&mask(right);
    bool increment=false;
    if(residue) {
        if(mode==numeric::Rounding::toward_positive) increment=!v.negative;
        if(mode==numeric::Rounding::toward_negative) increment=v.negative;
        if(mode==numeric::Rounding::nearest_ties_even && right<=64) {
            const auto half=std::uint64_t{1}<<(right-1);
            increment=residue>half || (residue==half && (integer&1));
        }
    }
    return {integer+static_cast<unsigned>(increment),residue!=0};
}
constexpr Result<Converted> overflow(Shape s, bool negative, Policy p) noexcept {
    if(p.precision==numeric::Precision::exact_only || p.overflow==Overflow::error)
        return std::unexpected(Error{ErrorCode::overflow});
    const auto sign=static_cast<std::uint64_t>(negative)<<(s.width-1);
    const auto inf=mask(s.exponent)<<s.fraction;
    const bool infinity=p.overflow==Overflow::ieee_result
        && (p.rounding==numeric::Rounding::nearest_ties_even
            || (p.rounding==numeric::Rounding::toward_negative && negative)
            || (p.rounding==numeric::Rounding::toward_positive && !negative));
    return Converted{sign|(infinity?inf:inf-1),Flags{false,true,true,false,false}};
}
} // namespace detail
constexpr Result<Decoded> decode_exact(Format format, std::uint64_t bits) noexcept {
    auto s=detail::shape(format); if(!s)return std::unexpected(s.error());
    if(bits&~detail::mask(s->width))return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool negative=(bits>>(s->width-1))!=0;
    const auto fraction=bits&detail::mask(s->fraction);
    const auto exponent=(bits>>s->fraction)&detail::mask(s->exponent);
    Decoded out{Category::zero,negative,{0,0,false},0,0};
    if(exponent==detail::mask(s->exponent)) {
        if(!fraction) out.category=Category::infinity;
        else {
            out.category=(fraction&(std::uint64_t{1}<<(s->fraction-1)))?Category::quiet_nan:Category::signaling_nan;
            out.payload=fraction&detail::mask(s->fraction-1);
            out.payload_bits=s->fraction-1;
        }
    } else if(exponent || fraction) {
        out.category=Category::finite;
        const auto magnitude=fraction|(exponent?std::uint64_t{1}<<s->fraction:0);
        const int q=(exponent?static_cast<int>(exponent):1)-s->bias-static_cast<int>(s->fraction);
        out.finite=*numeric::canonicalize({magnitude,static_cast<std::int16_t>(q),negative});
    }
    return out;
}
// Explicitly discards the sign of zero. Nonfinite values cannot become finite.
constexpr Result<numeric::BinaryValue> finite_value(const Decoded& v) noexcept {
    if(v.payload || v.payload_bits)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(v.category==Category::zero && v.finite==numeric::BinaryValue{0,0,false}) return v.finite;
    if(v.category==Category::finite && v.finite.magnitude && v.finite.negative==v.negative) {
        auto canonical=numeric::canonicalize(v.finite);
        if(canonical && *canonical==v.finite)return v.finite;
    }
    return std::unexpected(Error{ErrorCode::invalid_argument});
}
// Unlike canonicalize(), raw BinaryValue{0, exponent, true} explicitly requests -0.
constexpr Result<Converted> encode_finite(Format format, numeric::BinaryValue value, Policy policy) noexcept {
    auto s=detail::shape(format); if(!s)return std::unexpected(s.error());
    if(!detail::valid(policy))return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto sign=static_cast<std::uint64_t>(value.negative)<<(s->width-1);
    if(!value.magnitude)return Converted{sign,{}};
    int top=static_cast<int>(value.exponent)+static_cast<int>(std::bit_width(value.magnitude))-1;
    if(top>s->bias)return detail::overflow(*s,value.negative,policy);
    const int minimum=1-s->bias;
    // Tininess after rounding uses the target precision with an unbounded
    // exponent, before reducing precision onto the subnormal quantum.
    bool tiny = top < minimum;
    if (tiny) {
        const auto precision_rounded = detail::round(value, top-static_cast<int>(s->fraction), policy.rounding);
        const bool carry = precision_rounded.integer == (std::uint64_t{1}<<(s->fraction+1));
        tiny = top + static_cast<int>(carry) < minimum;
    }
    int q=(top<minimum?minimum:top)-static_cast<int>(s->fraction);
    const auto rounded=detail::round(value,q,policy.rounding);
    auto significand=rounded.integer;
    if(significand==(std::uint64_t{1}<<(s->fraction+1))) {
        significand>>=1; ++top;
    }
    if(top>s->bias)return detail::overflow(*s,value.negative,policy);
    if(rounded.inexact && policy.precision==numeric::Precision::exact_only)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool normal=significand>=(std::uint64_t{1}<<s->fraction);
    const auto exponent=normal?static_cast<std::uint64_t>((top<minimum?minimum:top)+s->bias):0;
    return Converted{sign|(exponent<<s->fraction)|(significand&detail::mask(s->fraction)),
                     Flags{false,rounded.inexact,false,rounded.inexact&&tiny,false}};
}
constexpr Result<Converted> convert(Format source, std::uint64_t bits, Format destination, Policy policy) noexcept {
    if(!detail::valid(policy))return std::unexpected(Error{ErrorCode::invalid_argument});
    auto src=decode_exact(source,bits); if(!src)return std::unexpected(src.error());
    auto dst=detail::shape(destination); if(!dst)return std::unexpected(dst.error());
    if(src->category==Category::finite)return encode_finite(destination,src->finite,policy);
    if(src->category==Category::zero)return encode_finite(destination,{0,0,src->negative},policy);
    if(policy.nonfinite==NonFinite::reject)return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto sign=static_cast<std::uint64_t>(src->negative)<<(dst->width-1);
    const auto inf=detail::mask(dst->exponent)<<dst->fraction;
    if(src->category==Category::infinity)return Converted{sign|inf,{}};
    const auto quiet=std::uint64_t{1}<<(dst->fraction-1);
    Flags flags{}; flags.invalid=src->category==Category::signaling_nan;
    if(policy.payload==NaNPayload::canonical) {
        flags.payload_loss=src->negative || src->payload!=0;
        return Converted{inf|quiet,flags};
    }
    auto payload=src->payload;
    const unsigned target=dst->fraction-1;
    if(src->payload_bits>target) {
        const unsigned shift=src->payload_bits-target;
        flags.payload_loss=(payload&detail::mask(shift))!=0;
        payload>>=shift;
    } else payload<<=target-src->payload_bits;
    if(flags.payload_loss && policy.payload==NaNPayload::exact)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return Converted{sign|inf|quiet|payload,flags};
}
} // namespace vita::codec::ieee
