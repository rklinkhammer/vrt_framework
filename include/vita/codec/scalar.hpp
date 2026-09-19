#pragma once
#include <vita/codec/wire.hpp>
#include <type_traits>

namespace vita::codec::detail {
// Descriptor variants are frozen alongside SemanticValue. All extents here are
// fixed scalars; variable structures deliberately have no inferred representation.
inline Result<SemanticValue> read_scalar(const FieldDescriptor& descriptor, Bytes bytes) noexcept {
    if (bytes.size() != descriptor.words * 4)
        return std::unexpected(Error{ErrorCode::short_input});
    const auto low = load32(bytes, 0);
    const auto bits = descriptor.words == 2
        ? (std::uint64_t{low} << 32) | load32(bytes, 4) : std::uint64_t{low};
    const auto signed16 = [](std::uint32_t word) noexcept {
        return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word));
    };
    const auto optional16=[&](std::uint32_t word,std::uint16_t sentinel=0x7fff) noexcept -> std::optional<std::int16_t> {
        if(static_cast<std::uint16_t>(word)==sentinel)return {};
        return signed16(word);
    };
    switch (descriptor.variant_index) {
    case 0: return SemanticValue{low};
    case 1: return SemanticValue{Hertz{std::bit_cast<std::int64_t>(bits)}};
    case 2: return SemanticValue{PayloadFormat{bits}};
    case 3: return SemanticValue{DecibelsQ7{signed16(low)}};
    case 4: return SemanticValue{GainStages{signed16(low), signed16(low >> 16)}};
    case 5: return SemanticValue{Femtoseconds{std::bit_cast<std::int64_t>(bits)}};
    case 6: return SemanticValue{CelsiusQ6{signed16(low)}};
    case 7: return SemanticValue{DeviceIdentifierValue{low & 0xffffffu,
                   static_cast<std::uint16_t>(bits)}};
    case 9: return SemanticValue{RadiansQ7{signed16(low)}};
    case 10: return SemanticValue{PolarizationAngles{signed16(low>>16),signed16(low)}};
    case 11: return SemanticValue{PointingAngles{static_cast<std::uint16_t>(low),signed16(low>>16)}};
    case 12: return SemanticValue{SpatialReferenceValue{static_cast<std::uint16_t>(low>>16),
                    static_cast<std::uint8_t>((low>>2)&3),static_cast<std::uint8_t>(low&3)}};
    case 13: return SemanticValue{BeamWidthCode{static_cast<std::uint16_t>(low>>16),static_cast<std::uint16_t>(low)}};
    case 14: return SemanticValue{MetresQ6{low}};
    case 15: return SemanticValue{EbNoBerValue{optional16(low>>16),optional16(low)}};
    case 16: return SemanticValue{ThresholdValue{signed16(low),signed16(low>>16)}};
    case 17: return SemanticValue{InterceptPointsValue{optional16(low>>16),optional16(low)}};
    case 18: return SemanticValue{SnrNoiseFigureValue{optional16(low>>16),optional16(low,0)}};
    case 19: return SemanticValue{Unsigned64Bits{bits}};
    case 20: return SemanticValue{VersionBuildValue{static_cast<std::uint8_t>(low>>25),
                    static_cast<std::uint16_t>((low>>16)&511),static_cast<std::uint8_t>((low>>10)&63),
                    static_cast<std::uint16_t>(low&1023)}};
    case 21: return SemanticValue{BufferSizeValue{low,static_cast<std::uint8_t>((bits>>8)&255),static_cast<std::uint8_t>(bits&255)}};
    case 22: return SemanticValue{CountryCodeValue{static_cast<std::uint16_t>(low&0x7ff),bool(low&0x8000)}};
    case 23: return SemanticValue{EmsDeviceClassValue{static_cast<std::uint16_t>(low&0xfff),
                    static_cast<std::uint8_t>((low>>14)&3),bool(low&0x2000),bool(low&0x1000)}};
    case 24: return SemanticValue{HumidityCode{static_cast<std::uint16_t>(low)}};
    case 25: return SemanticValue{BarometricPressureCode{low&0x1ffffu}};
    case 26: return SemanticValue{SeaSwellValue{static_cast<std::uint8_t>(low&31),static_cast<std::uint8_t>((low>>5)&31),static_cast<std::uint8_t>((low>>10)&63)}};
    case 27: return SemanticValue{TimestampDetailsValue{low,static_cast<std::uint32_t>(bits)}};
    default: return std::unexpected(Error{ErrorCode::unsupported_layout});
    }
}

inline std::uint64_t scalar_bits(const SemanticValue& value) noexcept {
    return std::visit([](const auto& scalar) noexcept -> std::uint64_t {
        using T = std::remove_cvref_t<decltype(scalar)>;
        const auto unsigned16=[](std::int16_t v) noexcept { return std::bit_cast<std::uint16_t>(v); };
        const auto optional16=[&](const OptionalQ7& v,std::uint16_t sentinel=0x7fff) noexcept {
            return v?unsigned16(*v):sentinel;
        };
        const auto pair=[](std::uint16_t high,std::uint16_t low) noexcept {
            return (std::uint32_t{high}<<16)|low;
        };
        if constexpr (std::is_same_v<T, std::uint32_t>) return scalar;
        else if constexpr (std::is_same_v<T, Hertz>) return std::bit_cast<std::uint64_t>(scalar.q20);
        else if constexpr (std::is_same_v<T, PayloadFormat>) return scalar.bits;
        else if constexpr (std::is_same_v<T, DecibelsQ7>) return std::bit_cast<std::uint16_t>(scalar.q7);
        else if constexpr (std::is_same_v<T, GainStages>)
            return (std::uint32_t{std::bit_cast<std::uint16_t>(scalar.stage2_q7)} << 16)
                 | std::bit_cast<std::uint16_t>(scalar.stage1_q7);
        else if constexpr (std::is_same_v<T, Femtoseconds>) return std::bit_cast<std::uint64_t>(scalar.count);
        else if constexpr (std::is_same_v<T, CelsiusQ6>) return std::bit_cast<std::uint16_t>(scalar.q6);
        else if constexpr (std::is_same_v<T, DeviceIdentifierValue>) return (std::uint64_t{scalar.oui} << 32) | scalar.device_code;
        else if constexpr (std::is_same_v<T, RadiansQ7>) return unsigned16(scalar.q7);
        else if constexpr (std::is_same_v<T, PolarizationAngles>) return pair(unsigned16(scalar.tilt_q7),unsigned16(scalar.ellipticity_q7));
        else if constexpr (std::is_same_v<T, PointingAngles>) return pair(unsigned16(scalar.elevation_q7),scalar.azimuth_q7);
        else if constexpr (std::is_same_v<T, SpatialReferenceValue>)
            return (std::uint32_t{scalar.user_id}<<16)|(std::uint32_t{scalar.reference}<<2)|scalar.beam;
        else if constexpr (std::is_same_v<T, BeamWidthCode>) return pair(scalar.horizontal_code,scalar.vertical_code);
        else if constexpr (std::is_same_v<T, MetresQ6>) return scalar.q6;
        else if constexpr (std::is_same_v<T, EbNoBerValue>) return pair(optional16(scalar.ebno_q7),optional16(scalar.ber_q7));
        else if constexpr (std::is_same_v<T, ThresholdValue>) return pair(unsigned16(scalar.stage2_q7),unsigned16(scalar.stage1_q7));
        else if constexpr (std::is_same_v<T, InterceptPointsValue>) return pair(optional16(scalar.second_q7),optional16(scalar.third_q7));
        else if constexpr (std::is_same_v<T, SnrNoiseFigureValue>) return pair(optional16(scalar.snr_q7),optional16(scalar.noise_figure_q7,0));
        else if constexpr (std::is_same_v<T, Unsigned64Bits>) return scalar.bits;
        else if constexpr (std::is_same_v<T, VersionBuildValue>)
            return (std::uint32_t{scalar.year_since2000}<<25)|(std::uint32_t{scalar.day}<<16)|
                   (std::uint32_t{scalar.revision}<<10)|scalar.user;
        else if constexpr (std::is_same_v<T, BufferSizeValue>)
            return (std::uint64_t{scalar.capacity_bytes}<<32)|(std::uint32_t{scalar.level}<<8)|scalar.status;
        else if constexpr (std::is_same_v<T, CountryCodeValue>) return (scalar.iso3166?0x8000u:0u)|scalar.code;
        else if constexpr (std::is_same_v<T, EmsDeviceClassValue>)
            return (std::uint32_t{scalar.organization}<<14)|(scalar.exciter?0x2000u:0u)|
                   (scalar.receiver?0x1000u:0u)|scalar.class_code;
        else if constexpr(std::is_same_v<T,HumidityCode>)return scalar.raw;
        else if constexpr(std::is_same_v<T,BarometricPressureCode>)return scalar.raw17;
        else if constexpr(std::is_same_v<T,SeaSwellValue>)return (std::uint32_t{scalar.user}<<10)|(std::uint32_t{scalar.swell}<<5)|scalar.sea;
        else if constexpr(std::is_same_v<T,TimestampDetailsValue>)return (std::uint64_t{scalar.flags}<<32)|scalar.epoch;
        else if constexpr(std::is_same_v<T,ProbabilityCode>)return (std::uint32_t{scalar.function}<<8)|scalar.value;
        else if constexpr(std::is_same_v<T,BeliefCode>)return scalar.value;
        else return 0; // Structured values use the owning snapshot extent/codec.
    }, value);
}

inline Result<SemanticValue> read_attribute_scalar(const FieldDescriptor& descriptor,Attribute attribute,Bytes bytes) noexcept {
    if(attribute==Attribute::probability || attribute==Attribute::belief) {
        if(bytes.size()!=4)return std::unexpected(Error{ErrorCode::short_input});
        const auto bits=load32(bytes,0);
        if(attribute==Attribute::probability) {if(bits&0xffff0000u)return std::unexpected(Error{ErrorCode::invalid_argument});return SemanticValue{ProbabilityCode{static_cast<std::uint8_t>(bits),static_cast<std::uint8_t>(bits>>8)}};}
        if(bits&0xffffff00u)return std::unexpected(Error{ErrorCode::invalid_argument});return SemanticValue{BeliefCode{static_cast<std::uint8_t>(bits)}};
    }
    return read_scalar(descriptor,bytes);
}
inline void write_scalar(const FieldDescriptor& descriptor, const SemanticValue& value,
                         MutableBytes output, std::size_t offset = 0) noexcept {
    const auto bits = scalar_bits(value);
    if (descriptor.words == 2) {
        store32(output, offset, static_cast<std::uint32_t>(bits >> 32));
        store32(output, offset + 4, static_cast<std::uint32_t>(bits));
    } else store32(output, offset, static_cast<std::uint32_t>(bits));
}
inline void write_attribute_scalar(const FieldDescriptor& descriptor,Attribute attribute,const SemanticValue& value,MutableBytes output,std::size_t offset=0) noexcept {
    if(attribute==Attribute::probability||attribute==Attribute::belief)store32(output,offset,static_cast<std::uint32_t>(scalar_bits(value)));
    else write_scalar(descriptor,value,output,offset);
}
} // namespace vita::codec::detail
