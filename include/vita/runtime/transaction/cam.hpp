#pragma once
#include <vita/codec/wire.hpp>
#include <vita/runtime/state/contracts.hpp>
namespace vita::runtime::transaction {
inline constexpr std::uint32_t not_executed=1u<<31,device_failure=1u<<30,unsupported=1u<<29,range_error=1u<<28,precision=1u<<27,invalid_value=1u<<26,timing_error=1u<<25;
inline constexpr std::uint32_t resource_exhausted=1u<<1,dependency_blocked=1u<<2,state_indeterminate=1u<<3;
enum class Profile { iq_generator_v1,generic_virtual_test,iq_frequency_tunable,graphx_radio };
struct Cam {
    std::uint32_t raw=0;unsigned action=0,timing=0;
    bool partial=false,allow_warning=false,allow_error=false,nack=false,request_v=false,request_x=false,request_s=false,detail_warning=false,detail_error=false;
    static Result<Cam> parse(const codec::Envelope& envelope,Profile profile) noexcept {
        if(envelope.type!=codec::PacketType::command||!envelope.command||envelope.ack||envelope.cancel)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto valid=codec::validate_cam(envelope);if(!valid)return std::unexpected(valid.error());const auto raw=envelope.command->cam;
        Cam c{raw,(raw>>23)&3,(raw>>12)&7,bool(raw&(1u<<27)),bool(raw&(1u<<26)),bool(raw&(1u<<25)),bool(raw&(1u<<22)),bool(raw&(1u<<20)),bool(raw&(1u<<19)),bool(raw&(1u<<18)),bool(raw&(1u<<17)),bool(raw&(1u<<16))};
        if(profile!=Profile::generic_virtual_test&&c.action==2&&c.request_s&&!c.request_x)return std::unexpected(Error{ErrorCode::unsupported_capability});
        return c;
    }
};
inline bool eligible(const Cam& cam,Diagnostics diagnostics,bool resolvable) noexcept {
    return resolvable&&(!diagnostics.warnings||cam.allow_warning)&&(!diagnostics.errors||cam.allow_error);
}
enum class AckKind : std::uint8_t { validation,execution,state };
inline bool should_emit(const Cam& cam,AckKind kind,Diagnostics summary) noexcept {
    const bool requested=kind==AckKind::validation?cam.request_v:kind==AckKind::execution?cam.request_x:cam.request_s;
    return requested&&(kind==AckKind::state||!cam.nack||summary.warnings||summary.errors);
}
struct Validation { SemanticValue adjusted{std::uint32_t{0}};Diagnostics diagnostics{};bool resolvable=true;std::uint8_t dependencies=0; };
inline Validation iq_validate(FieldId id,SemanticValue value) noexcept {
    Validation out{value};if(id!=SampleRate::id){out.diagnostics.errors=unsupported;out.resolvable=false;return out;}
    if(!std::holds_alternative<Hertz>(value)){out.diagnostics.errors=invalid_value;out.resolvable=false;return out;}
    const auto q=std::get<Hertz>(value).q20;
    constexpr std::int64_t unit=1ll<<20;
    if(q<unit||q>100'000'000ll*unit){out.diagnostics.errors=range_error;out.resolvable=false;return out;}
    auto integer=q/unit,remainder=q%unit;
    if(remainder){if(remainder>unit/2||(remainder==unit/2&&(integer&1)))++integer;out.adjusted=Hertz{integer*unit};out.diagnostics.warnings=precision;}
    return out;
}
inline Validation tunable_validate(FieldId id,SemanticValue value) noexcept {
    Validation out{value};
    if(id!=RFReferenceFrequency::id){out.diagnostics.errors=unsupported;out.resolvable=false;return out;}
    const auto* frequency=std::get_if<Hertz>(&value);
    constexpr std::int64_t unit=1ll<<20;
    if(!frequency||frequency->q20<static_cast<std::int64_t>(profiles::iq::minimum_center_hz)*unit||frequency->q20>static_cast<std::int64_t>(profiles::iq::maximum_center_hz)*unit||frequency->q20%unit){out.diagnostics.errors=range_error;out.resolvable=false;}
    return out;
}
inline Validation graphx_validate(FieldId id,SemanticValue value) noexcept {
    if(id==SampleRate::id){Validation out{value};const auto* rate=std::get_if<Hertz>(&value);constexpr std::int64_t unit=1ll<<20;if(!rate){out.diagnostics.errors=invalid_value;out.resolvable=false;}else if(rate->q20<1000ll*unit||rate->q20>2'000'000ll*unit){out.diagnostics.errors=range_error;out.resolvable=false;}else if(rate->q20%unit){out.diagnostics.warnings=precision;out.resolvable=false;}return out;}
    if(id==RFReferenceFrequency::id){Validation out{value};const auto* frequency=std::get_if<Hertz>(&value);constexpr std::int64_t unit=1ll<<20;if(!frequency){out.diagnostics.errors=invalid_value;out.resolvable=false;}else if(frequency->q20<static_cast<std::int64_t>(profiles::iq::minimum_center_hz)*unit||frequency->q20>static_cast<std::int64_t>(profiles::iq::maximum_center_hz)*unit){out.diagnostics.errors=range_error;out.resolvable=false;}return out;}
    Validation out{value};
    if(id==Bandwidth::id){const auto* bandwidth=std::get_if<Hertz>(&value);if(!bandwidth){out.diagnostics.errors=invalid_value;out.resolvable=false;}else if(bandwidth->q20<=0||bandwidth->q20>2'000'000ll*(1ll<<20)){out.diagnostics.errors=range_error;out.resolvable=false;}return out;}
    if(id==Gain::id){const auto* gain=std::get_if<GainStages>(&value);if(!gain||gain->stage2_q7){out.diagnostics.errors=invalid_value;out.resolvable=false;}else if(gain->stage1_q7<-60*128||gain->stage1_q7>60*128){out.diagnostics.errors=range_error;out.resolvable=false;}return out;}
    if(id==DiscreteIO32::id){const auto* discrete=std::get_if<std::uint32_t>(&value);if(!discrete||(*discrete!=2&&*discrete!=3)){out.diagnostics.errors=invalid_value;out.resolvable=false;}return out;}
    out.diagnostics.errors=unsupported;out.resolvable=false;return out;
}
} // namespace vita::runtime::transaction
