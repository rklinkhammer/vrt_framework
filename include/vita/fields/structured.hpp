#pragma once
#include <vita/core/bytes.hpp>
#include <vita/fields/cif2.hpp>
#include <vita/fields/cif3.hpp>
#include <vita/fields/cif1_structured.hpp>
#include <vita/core/error.hpp>
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

namespace vita {
struct EmbeddedFixTime {
    std::uint8_t tsi = 0, tsf = 0;
    std::uint32_t seconds = UINT32_MAX;
    std::uint64_t fractional = UINT64_MAX;
    friend bool operator==(const EmbeddedFixTime&, const EmbeddedFixTime&) = default;
};
struct GeolocationValue {
    std::uint32_t oui = 0xffffff;
    EmbeddedFixTime fix{};
    std::optional<std::int32_t> latitude_q22, longitude_q22, altitude_q5, speed_q16,
                               heading_q22, track_q22, magnetic_variation_q22;
    friend bool operator==(const GeolocationValue&, const GeolocationValue&) = default;
};
struct EphemerisValue {
    std::uint32_t oui = 0xffffff;
    EmbeddedFixTime fix{};
    std::array<std::optional<std::int32_t>,3> position_q5{}, attitude_q22{}, velocity_q16{};
    friend bool operator==(const EphemerisValue&, const EphemerisValue&) = default;
};
struct SentenceValidator {
    void* context = nullptr;
    Result<void> (*validate)(void*, std::span<const char>) noexcept = nullptr;
};
struct GpsAsciiInput {
    std::uint32_t oui = 0xffffff;
    std::span<const char> text{};
    SentenceValidator validator{};
};
struct AssociationListsInput {
    std::span<const std::uint32_t> source{}, system{}, vector{}, asynchronous{}, tags{};
    bool tags_present = false;
};
// Byte-backed accessors avoid manufacturing native uint32_t objects in an arena
// or assuming wire endianness/alignment. The owner must outlive each borrowed view.
class IntegerListView {
    Bytes bytes_{};
    bool wire_ = false;
public:
    IntegerListView() = default;
    IntegerListView(Bytes bytes, bool wire) noexcept : bytes_(bytes), wire_(wire) {}
    std::size_t size() const noexcept { return bytes_.size()/4; }
    Result<std::uint32_t> at(std::size_t index) const noexcept {
        if(index>=size())return std::unexpected(Error{ErrorCode::invalid_argument});
        std::uint32_t result=0;
        if(wire_)for(unsigned i=0;i<4;++i)result=(result<<8)|std::to_integer<unsigned>(bytes_[index*4+i]);
        else std::memcpy(&result,bytes_.data()+index*4,4);
        return result;
    }
};
struct GpsAsciiView { std::uint32_t oui; std::span<const char> text; };
struct AssociationListsView {
    IntegerListView source, system, vector, asynchronous, tags;
    bool tags_present = false;
};
namespace detail {
struct NativeAsciiHeader { std::uint32_t oui, characters; };
struct NativeAssociationHeader { std::array<std::uint32_t,5> counts{}; bool tags_present=false; };
// Internal native-arena access, not a checked wire decoder. The caller must supply
// bytes copied from a live T object by the typed builder. Size checks cannot
// validate arbitrary optional/bool object representations; wire APIs decode each
// integer/member separately before constructing native values.
template<class T> Result<T> native_object(Bytes bytes) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    if(bytes.size()<sizeof(T))return std::unexpected(Error{ErrorCode::invalid_argument});
    T result{};std::memcpy(&result,bytes.data(),sizeof(T));return result;
}
inline Result<void> validate_fix(std::uint32_t oui,const EmbeddedFixTime& fix) noexcept {
    if(oui>0xffffff || fix.tsi>3 || fix.tsf>3 ||
       (!fix.tsi && fix.seconds!=UINT32_MAX) || (!fix.tsf && fix.fractional!=UINT64_MAX) ||
       (fix.tsf==2 && fix.fractional>=1'000'000'000'000ull))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline bool numeric(const std::optional<std::int32_t>& v,std::int64_t low=INT32_MIN,std::int64_t high=INT32_MAX-1) noexcept {
    return !v || (*v>=low && *v<=high && *v!=INT32_MAX);
}
inline constexpr Result<void> validate_native(const UuidValue& value) noexcept {
    for(auto word:value.words)if(word)return {};
    return std::unexpected(Error{ErrorCode::invalid_argument});
}
inline Result<void> validate_native(const GeolocationValue& value) noexcept {
    auto fix=validate_fix(value.oui,value.fix);if(!fix)return fix;
    constexpr std::int64_t degree=1ll<<22;
    if(!numeric(value.latitude_q22,-90*degree,90*degree) ||
       !numeric(value.longitude_q22,-180*degree,180*degree) || !numeric(value.altitude_q5) ||
       !numeric(value.speed_q16,0) || !numeric(value.heading_q22,0,360*degree-1) ||
       !numeric(value.track_q22,0,360*degree-1) || !numeric(value.magnetic_variation_q22,-180*degree,180*degree))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<void> validate_native(const EphemerisValue& value) noexcept {
    auto fix=validate_fix(value.oui,value.fix);if(!fix)return fix;
    for(const auto* values:{&value.position_q5,&value.attitude_q22,&value.velocity_q16})
        for(const auto& item:*values)if(!numeric(item))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<void> validate_ascii(std::uint32_t oui,std::span<const char> text) noexcept {
    if(oui>0xffffff)return std::unexpected(Error{ErrorCode::invalid_argument});
    for(unsigned char c:text)if(c==0 || c>127)return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<void> validate_native(const GpsAsciiInput& value) noexcept {
    auto ascii=validate_ascii(value.oui,value.text);if(!ascii)return ascii;
    if(!value.validator.validate)return std::unexpected(Error{ErrorCode::unsupported_capability});
    return value.validator.validate(value.validator.context,value.text);
}
inline Result<void> validate_native(const AssociationListsInput& value) noexcept {
    if(value.source.size()>511 || value.system.size()>511 || value.vector.size()>65535 || value.asynchronous.size()>32767 ||
       (value.tags_present ? value.tags.size()!=value.asynchronous.size() : !value.tags.empty()))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto count=value.source.size()+value.system.size()+value.vector.size()+value.asynchronous.size()+value.tags.size();
    if(count>1024)return std::unexpected(Error{ErrorCode::resource_limit});
    return {};
}
template<class T> inline Result<void> validate_native_attribute(const T& value,bool current) noexcept {
    if(current)return validate_native(value);
    if constexpr(std::is_same_v<T,PointingVectorInput>)return validate_pointing(value,false);
    else if constexpr(std::is_same_v<T,SpectrumValue>)return validate_spectrum(value,false);
    else if constexpr(std::is_same_v<T,SectorStepScanInput>)return validate_sector(value,false);
    else if constexpr(std::is_same_v<T,UuidValue>)return {};
    else if constexpr(std::is_same_v<T,GeolocationValue>) {
        auto fix=validate_fix(value.oui,value.fix);if(!fix)return fix;
        for(const auto& v:{value.latitude_q22,value.longitude_q22,value.altitude_q5,value.speed_q16,value.heading_q22,value.track_q22,value.magnetic_variation_q22})
            if(!numeric(v))return std::unexpected(Error{ErrorCode::invalid_argument});
        return {};
    } else return validate_native(value);
}
inline Result<IndexListView> native_indices(Bytes bytes) noexcept {
    auto h=native_object<NativeIndexHeader>(bytes);if(!h)return std::unexpected(h.error());
    if(h->count>1024||bytes.size()!=sizeof(*h)+4*std::size_t{h->count}||(h->entry_bytes!=1&&h->entry_bytes!=2&&h->entry_bytes!=4))return std::unexpected(Error{ErrorCode::invalid_argument});
    return IndexListView{h->entry_bytes,h->count,bytes.subspan(sizeof(*h)),false};
}
inline Result<PointingVectorView> native_pointing(Bytes bytes) noexcept {
    auto h=native_object<NativePointingHeader>(bytes);if(!h)return std::unexpected(h.error());
    if(h->count>256||bytes.size()!=sizeof(*h)+sizeof(PointingVectorRecord)*std::size_t{h->count})return std::unexpected(Error{ErrorCode::invalid_argument});
    return PointingVectorView{h->global,h->record_reference,NativeRecordView<PointingVectorRecord>{bytes.subspan(sizeof(*h))}};
}
inline Result<SectorStepScanView> native_sectors(Bytes bytes) noexcept {
    auto h=native_object<NativeSectorHeader>(bytes);if(!h)return std::unexpected(h.error());
    if(h->count>256||bytes.size()!=sizeof(*h)+sizeof(SectorRecord)*std::size_t{h->count})return std::unexpected(Error{ErrorCode::invalid_argument});
    return SectorStepScanView{h->selectors,h->start_format,NativeRecordView<SectorRecord>{bytes.subspan(sizeof(*h))}};
}
inline Result<GpsAsciiView> native_ascii(Bytes bytes) noexcept {
    auto h=native_object<NativeAsciiHeader>(bytes);if(!h)return std::unexpected(h.error());
    if(h->characters!=bytes.size()-sizeof(*h))return std::unexpected(Error{ErrorCode::invalid_argument});
    auto text=std::span<const char>{reinterpret_cast<const char*>(bytes.data()+sizeof(*h)),h->characters};
    auto valid=validate_ascii(h->oui,text);if(!valid)return std::unexpected(valid.error());
    return GpsAsciiView{h->oui,text};
}
inline Result<AssociationListsView> native_associations(Bytes bytes) noexcept {
    auto h=native_object<NativeAssociationHeader>(bytes);if(!h)return std::unexpected(h.error());
    std::size_t total=0;for(auto n:h->counts){if(n>1024)return std::unexpected(Error{ErrorCode::resource_limit});total+=n;}
    if(total>1024 || bytes.size()!=sizeof(*h)+4*total || h->counts[0]>511 || h->counts[1]>511 ||
       (h->tags_present ? h->counts[4]!=h->counts[3] : h->counts[4]!=0))return std::unexpected(Error{ErrorCode::invalid_argument});
    std::array<IntegerListView,5> lists{};std::size_t offset=sizeof(*h);
    for(unsigned i=0;i<5;++i){lists[i]=IntegerListView{bytes.subspan(offset,h->counts[i]*4),false};offset+=h->counts[i]*4;}
    return AssociationListsView{lists[0],lists[1],lists[2],lists[3],lists[4],h->tags_present};
}
} // namespace detail
} // namespace vita
