#pragma once
#include <vita/core/bytes.hpp>
#include <vita/core/error.hpp>
#include <vita/fields/cif1.hpp>
#include <vita/fields/cif3.hpp>
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <span>
namespace vita {
struct IndexListInput { std::uint8_t entry_bytes=4;std::span<const std::uint32_t> entries{}; };
struct PointingReference {
    std::uint16_t index=0;std::uint8_t reference=0,beam=0;
    friend constexpr bool operator==(PointingReference,PointingReference)=default;
};
struct PointingVectorRecord {
    PointingAngles vector{};std::optional<PointingReference> reference{};
    friend constexpr bool operator==(PointingVectorRecord,PointingVectorRecord)=default;
};
struct PointingVectorInput {
    std::optional<PointingReference> global{};bool record_reference=false;
    std::span<const PointingVectorRecord> records{};
};
struct SpectrumValue {
    std::uint8_t spectrum_type=0,averaging=0,delta_kind=0;
    std::uint32_t window_type=0,transform_points=0,window_points=0;
    std::int64_t resolution_q20=0,span_q20=0;
    std::uint32_t averages=0,weighting_raw=0;
    std::int32_t f1_index=0,f2_index=0;
    std::uint32_t delta_raw=0;
    friend constexpr bool operator==(SpectrumValue,SpectrumValue)=default;
};
struct SectorStartTime {
    std::uint64_t fractional=0;std::uint32_t seconds=0;std::uint8_t tsi=0,tsf=0;
    friend constexpr bool operator==(SectorStartTime,SectorStartTime)=default;
};
struct SectorRecord {
    std::uint32_t sector=0;std::int64_t f1_q20=0;
    std::optional<std::int64_t> f2_q20{},bandwidth_q20{},step_q20{};
    std::optional<std::uint32_t> points{};
    std::optional<std::array<std::int16_t,2>> gain_q7{},threshold_q7{};
    std::optional<std::int64_t> dwell_fs{};
    std::optional<SectorStartTime> start{};
    std::optional<std::int64_t> time3_fs{},time4_fs{};
    friend constexpr bool operator==(SectorRecord,SectorRecord)=default;
};
struct SectorStepScanInput {
    std::uint32_t selectors=0xc0000000u;
    TimestampFormatBinding start_format{};
    std::span<const SectorRecord> records{};
};
struct PointingVectorView;
struct SectorStepScanView;
namespace detail {
// Internal arena readers: Bytes must hold copied native objects, never wire input.
Result<PointingVectorView> native_pointing(Bytes) noexcept;
Result<SectorStepScanView> native_sectors(Bytes) noexcept;
}
template<class T> requires (std::is_same_v<T,PointingVectorRecord> || std::is_same_v<T,SectorRecord>)
class NativeRecordView {
    static_assert(std::is_trivially_copyable_v<T>);
    Bytes bytes_{};
    explicit NativeRecordView(Bytes bytes) noexcept:bytes_(bytes){}
    friend Result<PointingVectorView> detail::native_pointing(Bytes) noexcept;
    friend Result<SectorStepScanView> detail::native_sectors(Bytes) noexcept;
public:
    NativeRecordView()=default;
    std::size_t size() const noexcept{return bytes_.size()/sizeof(T);}
    Result<T> at(std::size_t index) const noexcept {
        if(bytes_.size()%sizeof(T)||index>=size())return std::unexpected(Error{ErrorCode::invalid_argument});
        T value{};std::memcpy(&value,bytes_.data()+index*sizeof(T),sizeof(T));return value;
    }
};
struct IndexListView {
    std::uint8_t entry_bytes=4;std::size_t count=0;Bytes bytes{};bool wire=false;
    std::size_t size() const noexcept{return count;}
    Result<std::uint32_t> at(std::size_t index) const noexcept {
        if((entry_bytes!=1&&entry_bytes!=2&&entry_bytes!=4)||index>=count)return std::unexpected(Error{ErrorCode::invalid_argument});
        const std::size_t width=wire?entry_bytes:4;
        if(index>SIZE_MAX/width || index*width>bytes.size() || width>bytes.size()-index*width)return std::unexpected(Error{ErrorCode::short_input});
        std::uint32_t value=0;if(wire)for(unsigned i=0;i<width;++i)value=(value<<8)|std::to_integer<unsigned>(bytes[index*width+i]);
        else std::memcpy(&value,bytes.data()+index*width,4);return value;
    }
};
struct PointingVectorView { std::optional<PointingReference> global{};bool record_reference=false;NativeRecordView<PointingVectorRecord> records{}; };
struct SectorStepScanView { std::uint32_t selectors=0;TimestampFormatBinding start_format{};NativeRecordView<SectorRecord> records{}; };
namespace detail {
struct NativeIndexHeader {std::uint32_t count=0;std::uint8_t entry_bytes=4;};
struct NativePointingHeader {std::uint32_t count=0;std::optional<PointingReference> global{};bool record_reference=false;};
struct NativeSectorHeader {std::uint32_t count=0,selectors=0;TimestampFormatBinding start_format{};};
struct RecordShape {std::size_t header_words=0,record_words=0,records=0,total_words=0,work=0,header_code=0;};
constexpr Result<RecordShape> pointing_shape(bool global,bool reference,std::size_t count) noexcept {
    if(count>4095)return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto h=global?4u:3u,r=reference?2u:1u;
    return RecordShape{h,r,count,h+r*count,h+r*count,h};
}
constexpr Result<RecordShape> sector_shape(std::uint32_t selectors,std::size_t count,TimestampFormatBinding binding) noexcept {
    if((selectors&0xc0000000u)!=0xc0000000u || (selectors&0x000fffffu)||count>4095)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    std::size_t words=3;
    constexpr std::array<unsigned,10> widths{2,2,2,1,1,1,2,0,2,2};
    for(unsigned i=0;i<10;++i)if(selectors&(1u<<(29-i))) {
        if(i==7){if(binding.tsi>3||binding.tsf>3)return std::unexpected(Error{ErrorCode::invalid_argument});
            if(!binding.bound||(!binding.tsi&&!binding.tsf))return std::unexpected(Error{ErrorCode::unsupported_capability});
            words+=(binding.tsi?1:0)+(binding.tsf?2:0);
        }else words+=widths[i];
    }
    return RecordShape{3,words,count,3+words*count,3+words*count,0};
}
constexpr Result<void> validate_reference(PointingReference v,bool global=false) noexcept {
    if(v.reference>3||v.beam>2||(global&&v.index))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<void> validate_native(const IndexListInput& v) noexcept {
    if(v.entry_bytes!=1&&v.entry_bytes!=2&&v.entry_bytes!=4)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(v.entries.size()>1024)return std::unexpected(Error{ErrorCode::resource_limit});
    for(auto value:v.entries)if(v.entry_bytes<4 && value>=(std::uint32_t{1}<<(8*v.entry_bytes)))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<void> validate_pointing(const PointingVectorInput& v,bool current) noexcept {
    if(v.records.size()>256)return std::unexpected(Error{ErrorCode::resource_limit});
    if(v.global){auto r=validate_reference(*v.global,true);if(!r)return r;}
    for(const auto& record:v.records) {
        if(record.reference.has_value()!=v.record_reference)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(record.reference){auto r=validate_reference(*record.reference);if(!r)return r;}
        if(current&&(record.vector.elevation_q7 < -90*128 || record.vector.elevation_q7>90*128))return std::unexpected(Error{ErrorCode::invalid_argument});
    }return {};
}
inline Result<void> validate_native(const PointingVectorInput& v) noexcept{return validate_pointing(v,true);}
constexpr Result<void> validate_spectrum(SpectrumValue v,bool current) noexcept {
    if((v.spectrum_type>4&&v.spectrum_type<128)||(v.averaging&0xc0)||v.delta_kind>3||(v.window_type>=44&&v.window_type<100))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(current&&(v.averaging==32||v.resolution_q20<0||v.span_q20<0||(v.delta_kind==1&&std::bit_cast<std::int32_t>(v.delta_raw)>100*4096)))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
constexpr Result<void> validate_native(SpectrumValue v) noexcept{return validate_spectrum(v,true);}
inline Result<void> validate_sector(const SectorStepScanInput& v,bool current) noexcept {
    if(v.records.size()>256)return std::unexpected(Error{ErrorCode::resource_limit});
    auto shape=sector_shape(v.selectors,v.records.size(),v.start_format);if(!shape)return std::unexpected(shape.error());
    for(const auto& record:v.records) {
        const std::array present{bool(record.f2_q20),bool(record.bandwidth_q20),bool(record.step_q20),bool(record.points),bool(record.gain_q7),bool(record.threshold_q7),bool(record.dwell_fs),bool(record.start),bool(record.time3_fs),bool(record.time4_fs)};
        for(unsigned i=0;i<10;++i)if(present[i]!=bool(v.selectors&(1u<<(29-i))))return std::unexpected(Error{ErrorCode::invalid_argument});
        if(record.start) {
            const auto t=*record.start;
            if(t.tsi!=v.start_format.tsi||t.tsf!=v.start_format.tsf||(!t.tsi&&t.seconds)||(!t.tsf&&t.fractional))return std::unexpected(Error{ErrorCode::invalid_argument});
        }
        if(current&&(record.f1_q20<0||(record.f2_q20&&*record.f2_q20<0)||(record.bandwidth_q20&&*record.bandwidth_q20<0)||(record.step_q20&&*record.step_q20<0)||(record.dwell_fs&&*record.dwell_fs<0)))return std::unexpected(Error{ErrorCode::invalid_argument});
    }return {};
}
inline Result<void> validate_native(const SectorStepScanInput& v) noexcept{return validate_sector(v,true);}
} // namespace detail
} // namespace vita
