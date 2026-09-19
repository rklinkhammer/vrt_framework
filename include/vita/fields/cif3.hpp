#pragma once
#include <vita/core/error.hpp>
#include <cstdint>
namespace vita {
struct TimestampFormatBinding {
    std::uint8_t tsi=0,tsf=0;
    bool bound=false;
    friend constexpr bool operator==(TimestampFormatBinding,TimestampFormatBinding)=default;
};
struct StateDurationValue {
    std::uint64_t fractional=0;
    std::uint32_t seconds=0;
    std::uint8_t tsi=0,tsf=0;
    friend constexpr bool operator==(StateDurationValue,StateDurationValue)=default;
};
struct HumidityCode { std::uint16_t raw=0; friend constexpr bool operator==(HumidityCode,HumidityCode)=default; };
// Raw seventeen-bit code only: accepted D-P14 policy does not infer a Pascal scale.
struct BarometricPressureCode { std::uint32_t raw17=0; friend constexpr bool operator==(BarometricPressureCode,BarometricPressureCode)=default; };
struct SeaSwellValue { std::uint8_t sea=0,swell=0,user=0; friend constexpr bool operator==(SeaSwellValue,SeaSwellValue)=default; };
struct TimestampDetailsValue {
    std::uint32_t flags=0,epoch=0;
    constexpr unsigned epoch_code() const noexcept { return (flags>>16)&3; }
    constexpr unsigned leap_handling() const noexcept { return (flags>>14)&3; }
    constexpr unsigned leap_prediction() const noexcept { return (flags>>12)&3; }
    constexpr unsigned source() const noexcept { return (flags>>9)&7; }
    constexpr bool global() const noexcept { return flags&(1u<<18); }
    constexpr bool offset_enabled() const noexcept { return flags&(1u<<8); }
    friend constexpr bool operator==(TimestampDetailsValue,TimestampDetailsValue)=default;
};
constexpr Result<void> validate_timestamp_details_intrinsic(TimestampDetailsValue v) noexcept {
    if((v.flags&0x00f80000u) || (v.leap_handling()==0 && v.leap_prediction()!=0 && v.leap_prediction()!=2))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
constexpr Result<TimestampDetailsValue> make_timestamp_details(std::uint32_t flags,std::uint32_t epoch) noexcept {
    TimestampDetailsValue v{flags,epoch};auto valid=validate_timestamp_details_intrinsic(v);
    if(!valid)return std::unexpected(valid.error());return v;
}
// Mask bit n denotes observed TSI code n. Incomplete observations cannot prove validity.
struct TimestampDetailsScope {
    std::uint8_t observed_tsi_mask=0;
    bool scope_complete=false;
    std::uint8_t documented_user_bits=0;
    bool user_source_documented=false;
    std::uint8_t observed_tsf_mask=0;
};
enum class TimestampScopeStatus { complete,incomplete,not_applicable };
constexpr Result<TimestampScopeStatus> validate_timestamp_details(TimestampDetailsValue v,TimestampDetailsScope scope) noexcept {
    auto valid=validate_timestamp_details_intrinsic(v);if(!valid)return std::unexpected(valid.error());
    if((scope.observed_tsi_mask|scope.observed_tsf_mask)&0xf0)return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool utc=scope.observed_tsi_mask&2,gps=scope.observed_tsi_mask&4;
    const auto code=v.epoch_code();
    bool epoch_ok=true;
    if(utc&&gps)epoch_ok=code==0;
    else if(utc)epoch_ok=code==0 || ((code==1||code==3)&&v.epoch==0);
    else if(gps)epoch_ok=code==0 || (code==1&&v.epoch==315964811u) || (code==2&&v.epoch==0) || (code==3&&v.epoch==315964800u);
    if(!epoch_ok)return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool undocumented=((v.flags>>24)&~scope.documented_user_bits)!=0 || (v.source()>=6&&!scope.user_source_documented);
    if(!scope.scope_complete || undocumented)return TimestampScopeStatus::incomplete;
    if(!((scope.observed_tsi_mask|scope.observed_tsf_mask)&14))return TimestampScopeStatus::not_applicable;
    return TimestampScopeStatus::complete;
}
namespace detail {
constexpr Result<void> validate_native(StateDurationValue v) noexcept {
    if(v.tsi>3||v.tsf>3||(!v.tsi&&!v.tsf)||(!v.tsi&&v.seconds)||(!v.tsf&&v.fractional))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
}
} // namespace vita
