#pragma once
#include <vita/codec/general_samples.hpp>
#include <vita/codec/ieee_samples.hpp>
#include <vita/fields/types.hpp>
#include <optional>
#include <variant>

namespace vita::codec::samples {
enum class SignalDomain { time, spectral, spectral_log_power };
enum class ComponentUnit { dimensionless, decibels, pi_multiple, two_pi_multiple, radians, unspecified };
using NumberFormat = std::variant<numeric::NumericSpec, ieee::Format>;
class Descriptor {
    PayloadFormat wire_;
    general::PackingSpec packing_;
    NumberFormat number_;
    SignalDomain domain_;
    Descriptor(PayloadFormat w,general::PackingSpec p,NumberFormat n,SignalDomain d) noexcept
        :wire_(w),packing_(p),number_(n),domain_(d){}
    friend Result<Descriptor> descriptor(PayloadFormat,SignalDomain) noexcept;
public:
    PayloadFormat wire() const noexcept{return wire_;}
    general::PackingSpec packing() const noexcept{return packing_;}
    const NumberFormat& number() const noexcept{return number_;}
    SignalDomain domain() const noexcept{return domain_;}
};
inline Result<Descriptor> descriptor(PayloadFormat wire,SignalDomain domain) noexcept {
    if(domain!=SignalDomain::time && domain!=SignalDomain::spectral && domain!=SignalDomain::spectral_log_power)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto high=static_cast<std::uint32_t>(wire.bits>>32);
    const auto low=static_cast<std::uint32_t>(wire.bits);
    const auto kind=(high>>29)&3u,code=(high>>24)&31u,frac=(high>>12)&15u;
    if(kind==3)return std::unexpected(Error{ErrorCode::invalid_argument});
    general::PackingSpec p{};
    p.item_bits=(high&63u)+1;p.packing_bits=((high>>6)&63u)+1;
    p.channel_tag_bits=(high>>16)&15u;p.event_tag_bits=(high>>20)&7u;
    p.packing=(high>>31)?general::PackingMode::link:general::PackingMode::processing;
    p.kind=kind==0?general::SampleKind::real:kind==1?general::SampleKind::cartesian:general::SampleKind::polar;
    p.repeat_count=(low>>16)+1;p.vector_size=(low&65535u)+1;
    const bool component=(high&(1u<<23))!=0;
    if(component && p.repeat_count==1)return std::unexpected(Error{ErrorCode::invalid_argument});
    p.repeating=component?general::RepeatMode::component:p.repeat_count>1?general::RepeatMode::channel:general::RepeatMode::none;
    auto shape=general::measure(p,0);if(!shape)return std::unexpected(shape.error());
    NumberFormat number{};
    if(code==13 || code==14 || code==15) {
        const unsigned bits=code==13?16:code==14?32:64;
        if(p.item_bits!=bits || frac)return std::unexpected(Error{ErrorCode::invalid_argument});
        number=code==13?ieee::Format::binary16:code==14?ieee::Format::binary32:ieee::Format::binary64;
    } else {
        numeric::NumericSpec n{numeric::Kind::signed_fixed,p.item_bits};
        if(code==0 || code==16)n.kind=code==0?numeric::Kind::signed_fixed:numeric::Kind::unsigned_fixed;
        else if((code>=1 && code<=6) || (code>=17 && code<=22)) {
            n.kind=code<16?numeric::Kind::signed_vrt:numeric::Kind::unsigned_vrt;
            n.exponent_bits=code&15u;
        } else if(code==7 || code==23) {
            n.kind=code==7?numeric::Kind::signed_non_normalized:numeric::Kind::unsigned_non_normalized;
            n.fractional_bits=frac;
            if(domain==SignalDomain::time)return std::unexpected(Error{ErrorCode::invalid_argument});
        } else return std::unexpected(Error{ErrorCode::unsupported_layout});
        if(code!=7 && code!=23 && frac)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto zero=numeric::decode_exact(n,0);if(!zero)return std::unexpected(zero.error());
        number=n;
    }
    if(domain==SignalDomain::spectral_log_power) {
        if(p.kind!=general::SampleKind::real || p.item_bits>16 || (code!=0 && code!=7 && code!=23))
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    return Descriptor{wire,p,number,domain};
}
inline Result<ComponentUnit> component_unit(const Descriptor& d,unsigned component) noexcept {
    const auto p=d.packing();
    if(component>=(p.kind==general::SampleKind::real?1u:2u))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(d.domain()==SignalDomain::spectral_log_power)return ComponentUnit::decibels;
    if(p.kind!=general::SampleKind::polar || component!=1)return ComponentUnit::dimensionless;
    if(std::holds_alternative<ieee::Format>(d.number()))return ComponentUnit::radians;
    const auto kind=std::get<numeric::NumericSpec>(d.number()).kind;
    if(kind==numeric::Kind::signed_non_normalized || kind==numeric::Kind::unsigned_non_normalized)return ComponentUnit::unspecified;
    return kind==numeric::Kind::signed_fixed || kind==numeric::Kind::signed_vrt?ComponentUnit::pi_multiple:ComponentUnit::two_pi_multiple;
}
enum class PadReporting { omitted_by_class, exact, allow_zero_when_implied };
struct PaddingEvidence { PadReporting policy; std::optional<std::uint8_t> class_id_pad_bits; };
struct PayloadBinding { std::size_t structures; PaddingEvidence padding; };
inline Result<general::Shape> measure_payload(const Descriptor& d,PayloadBinding binding,general::Limits limits={}) noexcept {
    auto shape=general::measure(d.packing(),binding.structures,limits);if(!shape)return std::unexpected(shape.error());
    const auto p=d.packing();
    const auto last=shape->items?general::detail::bit_offset(p,shape->items-1)+p.packing_bits:0;
    const auto padding=shape->bytes*8-last;
    const auto evidence=binding.padding;
    if(evidence.policy==PadReporting::omitted_by_class) {
        if(evidence.class_id_pad_bits)return std::unexpected(Error{ErrorCode::invalid_argument});
    } else if(evidence.policy==PadReporting::exact || evidence.policy==PadReporting::allow_zero_when_implied) {
        if(!evidence.class_id_pad_bits || *evidence.class_id_pad_bits>31)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(*evidence.class_id_pad_bits!=padding && !(evidence.policy==PadReporting::allow_zero_when_implied
            && *evidence.class_id_pad_bits==0 && padding<p.item_bits))return std::unexpected(Error{ErrorCode::invalid_argument});
    } else return std::unexpected(Error{ErrorCode::invalid_argument});
    return shape;
}
inline Result<general::PackedSamples> contiguous(Bytes bytes,const Descriptor& d,PayloadBinding binding,general::Limits limits={}) noexcept {
    auto shape=measure_payload(d,binding,limits);if(!shape)return std::unexpected(shape.error());
    return general::PackedSamples::create(bytes,d.packing(),binding.structures,limits);
}
} // namespace vita::codec::samples
