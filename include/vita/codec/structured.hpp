#pragma once
#include <vita/codec/wire.hpp>
#include <vita/codec/cif1_structured.hpp>
#include <vita/fields/structured.hpp>

namespace vita::codec {
namespace detail {
inline EmbeddedFixTime read_fix(Bytes bytes) noexcept {
    const auto header=load32(bytes,0);
    return EmbeddedFixTime{static_cast<std::uint8_t>((header>>26)&3),static_cast<std::uint8_t>((header>>24)&3),
        load32(bytes,4),(std::uint64_t{load32(bytes,8)}<<32)|load32(bytes,12)};
}
inline std::optional<std::int32_t> read_known(Bytes bytes,std::size_t offset) noexcept {
    const auto raw=load32(bytes,offset);
    if(raw==0x7fffffffu)return {};
    return std::bit_cast<std::int32_t>(raw);
}
inline Result<FieldExtent> wire_structure_extent(FieldId id,Bytes bytes,TimestampFormatBinding binding={}) noexcept {
    if(cif1_structure(id))return cif1_wire_extent(id,bytes,binding);
    std::size_t size=0,units=0;
    if(temporal_duration_field(id)) {
        auto extent=duration_extent(binding);if(!extent)return std::unexpected(extent.error());
        size=extent->bytes;units=extent->units;
    }
    else if(id==ControlleeUUID::id || id==ControllerUUID::id){size=16;units=4;}
    else if(id==FormattedGPS::id || id==FormattedINS::id){size=44;units=11;}
    else if(id==ECEFEphemeris::id || id==RelativeEphemeris::id){size=52;units=13;}
    else {
        if(bytes.size()<8)return std::unexpected(Error{ErrorCode::short_input,0,8});
        const auto first=load32(bytes,0),second=load32(bytes,4);
        if(id==GPSASCII::id) {
            if(first&0xff000000u)return std::unexpected(Error{ErrorCode::invalid_argument});
            auto data=checked_multiply(second,4);if(!data)return std::unexpected(data.error());
            auto total=checked_add(*data,8);if(!total)return std::unexpected(total.error());
            size=*total;units=*data+2;
        } else if(id==ContextAssociationLists::id) {
            if(first&0xfe00fe00u)return std::unexpected(Error{ErrorCode::invalid_argument});
            const std::size_t count=((first>>16)&511)+(first&511)+(second>>16)+(second&32767)+
                ((second&0x8000)?(second&32767):0);
            size=8+4*count;units=2+count;
        } else return std::unexpected(Error{ErrorCode::unsupported_layout});
    }
    if(bytes.size()<size)return std::unexpected(Error{ErrorCode::short_input,0,size});
    return FieldExtent{size,units};
}
inline Result<void> validate_structure_wire(FieldId id,Bytes bytes,TimestampFormatBinding binding={}) noexcept {
    if(cif1_structure(id))return validate_cif1_wire(id,bytes,binding);
    auto extent=wire_structure_extent(id,bytes,binding);if(!extent)return std::unexpected(extent.error());
    if(extent->bytes!=bytes.size())return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id.cif==0 && id.bit>=11 && id.bit<=14) {
        if(load32(bytes,0)&0xf0000000u)return std::unexpected(Error{ErrorCode::invalid_argument});
        return vita::detail::validate_fix(load32(bytes,0)&0xffffffu,read_fix(bytes));
    }
    if(id==GPSASCII::id) {
        bool padding=false;std::size_t pad=0;
        for(auto byte:bytes.subspan(8)) {
            const auto c=std::to_integer<unsigned>(byte);
            if(!c){padding=true;++pad;}
            else if(padding || c>127)return std::unexpected(Error{ErrorCode::invalid_argument});
        }
        if(pad>3)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    return {};
}
inline Result<StateDurationValue> wire_duration(Bytes bytes,TimestampFormatBinding binding) noexcept {
    auto valid=validate_structure_wire(Age::id,bytes,binding);if(!valid)return std::unexpected(valid.error());
    StateDurationValue value{};value.tsi=binding.tsi;value.tsf=binding.tsf;
    std::size_t offset=0;
    if(binding.tsi){value.seconds=load32(bytes,offset);offset+=4;}
    if(binding.tsf)value.fractional=(std::uint64_t{load32(bytes,offset)}<<32)|load32(bytes,offset+4);
    return value;
}
inline Result<UuidValue> wire_uuid(Bytes bytes) noexcept {
    auto valid=validate_structure_wire(ControlleeUUID::id,bytes);if(!valid)return std::unexpected(valid.error());
    UuidValue value;for(unsigned i=0;i<4;++i)value.words[i]=load32(bytes,i*4);return value;
}
inline Result<GeolocationValue> wire_geolocation(Bytes bytes) noexcept {
    auto valid=validate_structure_wire(FormattedGPS::id,bytes);if(!valid)return std::unexpected(valid.error());
    GeolocationValue result{};result.oui=load32(bytes,0)&0xffffffu;result.fix=read_fix(bytes);
    result.latitude_q22=read_known(bytes,16);result.longitude_q22=read_known(bytes,20);
    result.altitude_q5=read_known(bytes,24);result.speed_q16=read_known(bytes,28);
    result.heading_q22=read_known(bytes,32);result.track_q22=read_known(bytes,36);result.magnetic_variation_q22=read_known(bytes,40);
    return result;
}
inline Result<EphemerisValue> wire_ephemeris(Bytes bytes) noexcept {
    auto valid=validate_structure_wire(ECEFEphemeris::id,bytes);if(!valid)return std::unexpected(valid.error());
    EphemerisValue result{};result.oui=load32(bytes,0)&0xffffffu;result.fix=read_fix(bytes);
    for(unsigned i=0;i<3;++i){result.position_q5[i]=read_known(bytes,16+4*i);result.attitude_q22[i]=read_known(bytes,28+4*i);result.velocity_q16[i]=read_known(bytes,40+4*i);}
    return result;
}
inline Result<GpsAsciiView> wire_ascii(Bytes bytes) noexcept {
    auto valid=validate_structure_wire(GPSASCII::id,bytes);if(!valid)return std::unexpected(valid.error());
    auto count=bytes.size()-8;while(count && bytes[8+count-1]==std::byte{})--count;
    return GpsAsciiView{load32(bytes,0)&0xffffffu,{reinterpret_cast<const char*>(bytes.data()+8),count}};
}
inline Result<AssociationListsView> wire_associations(Bytes bytes) noexcept {
    auto valid=validate_structure_wire(ContextAssociationLists::id,bytes);if(!valid)return std::unexpected(valid.error());
    const auto a=load32(bytes,0),b=load32(bytes,4);const bool tags=b&0x8000;
    const std::array<std::size_t,5> counts{(a>>16)&511,a&511,b>>16,b&32767,tags?(b&32767):0};
    std::array<IntegerListView,5> lists{};std::size_t offset=8;
    for(unsigned i=0;i<5;++i){lists[i]={bytes.subspan(offset,counts[i]*4),true};offset+=counts[i]*4;}
    return AssociationListsView{lists[0],lists[1],lists[2],lists[3],lists[4],tags};
}
inline void write_fix(std::uint32_t oui,const EmbeddedFixTime& fix,MutableBytes output) noexcept {
    store32(output,0,oui|(std::uint32_t{fix.tsi}<<26)|(std::uint32_t{fix.tsf}<<24));
    store32(output,4,fix.seconds);store32(output,8,static_cast<std::uint32_t>(fix.fractional>>32));store32(output,12,static_cast<std::uint32_t>(fix.fractional));
}
inline void write_known(const std::optional<std::int32_t>& value,MutableBytes output,std::size_t offset) noexcept {
    store32(output,offset,value?std::bit_cast<std::uint32_t>(*value):0x7fffffffu);
}
inline Result<void> write_structure(FieldId id,Bytes native,MutableBytes output,TimestampFormatBinding binding={},Attribute attribute=Attribute::current) noexcept {
    if(cif1_structure(id))return write_cif1(id,native,output,binding,attribute);
    if(temporal_duration_field(id)) {
        auto extent=native_structure_extent(id,native,binding,attribute);if(!extent)return std::unexpected(extent.error());
        if(output.size()<extent->bytes)return std::unexpected(Error{ErrorCode::short_output,0,extent->bytes});
        auto value=vita::detail::native_object<StateDurationValue>(native);if(!value)return std::unexpected(value.error());
        std::size_t offset=0;
        if(binding.tsi){store32(output,offset,value->seconds);offset+=4;}
        if(binding.tsf){store32(output,offset,static_cast<std::uint32_t>(value->fractional>>32));store32(output,offset+4,static_cast<std::uint32_t>(value->fractional));}
        return {};
    }
    if(id==ControlleeUUID::id || id==ControllerUUID::id) {
        auto value=vita::detail::native_object<UuidValue>(native);if(!value)return std::unexpected(value.error());
        for(unsigned i=0;i<4;++i)store32(output,i*4,value->words[i]);
    } else if(id==FormattedGPS::id || id==FormattedINS::id) {
        auto v=vita::detail::native_object<GeolocationValue>(native);if(!v)return std::unexpected(v.error());
        write_fix(v->oui,v->fix,output);
        const std::array fields{v->latitude_q22,v->longitude_q22,v->altitude_q5,v->speed_q16,v->heading_q22,v->track_q22,v->magnetic_variation_q22};
        for(unsigned i=0;i<7;++i)write_known(fields[i],output,16+4*i);
    } else if(id==ECEFEphemeris::id || id==RelativeEphemeris::id) {
        auto v=vita::detail::native_object<EphemerisValue>(native);if(!v)return std::unexpected(v.error());
        write_fix(v->oui,v->fix,output);
        for(unsigned i=0;i<3;++i){write_known(v->position_q5[i],output,16+4*i);write_known(v->attitude_q22[i],output,28+4*i);write_known(v->velocity_q16[i],output,40+4*i);}
    } else if(id==GPSASCII::id) {
        auto v=vita::detail::native_ascii(native);if(!v)return std::unexpected(v.error());
        store32(output,0,v->oui);store32(output,4,static_cast<std::uint32_t>((v->text.size()+3)/4));
        for(std::size_t i=8;i<output.size();++i)output[i]=std::byte{};
        for(std::size_t i=0;i<v->text.size();++i)output[8+i]=std::byte(static_cast<unsigned char>(v->text[i]));
    } else if(id==ContextAssociationLists::id) {
        auto v=vita::detail::native_associations(native);if(!v)return std::unexpected(v.error());
        store32(output,0,static_cast<std::uint32_t>((v->source.size()<<16)|v->system.size()));
        store32(output,4,static_cast<std::uint32_t>((v->vector.size()<<16)|(v->tags_present?0x8000:0)|v->asynchronous.size()));
        const std::array lists{v->source,v->system,v->vector,v->asynchronous,v->tags};std::size_t offset=8;
        for(auto list:lists)for(std::size_t i=0;i<list.size();++i){store32(output,offset,*list.at(i));offset+=4;}
    } else return std::unexpected(Error{ErrorCode::unsupported_layout});
    return {};
}
} // namespace detail
} // namespace vita::codec
