#pragma once
#include <vita/codec/wire.hpp>
namespace vita::codec {
namespace detail {
inline bool cif1_structure(FieldId id) noexcept {return id==IndexList::id||id==PointingVectorStructure::id||id==Spectrum::id||id==SectorStepScan::id;}
inline Result<vita::detail::RecordShape> wire_record_shape(FieldId id,Bytes bytes,TimestampFormatBinding binding) noexcept {
    if(bytes.size()<12)return std::unexpected(Error{ErrorCode::short_input,0,12});
    const auto total=load32(bytes,0),description=load32(bytes,4),selectors=load32(bytes,8);
    auto declared=checked_multiply(total,4);if(!declared)return std::unexpected(declared.error());
    if(bytes.size()<*declared)return std::unexpected(Error{ErrorCode::short_input,0,*declared});
    if(total<3)return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto h=description>>24,r=(description>>12)&4095,n=description&4095;
    Result<vita::detail::RecordShape> shape=std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==PointingVectorStructure::id) {
        if((h!=3&&h!=4)||(selectors&0x3fffffffu)||!(selectors&(1u<<30)))return std::unexpected(Error{ErrorCode::invalid_argument});
        shape=vita::detail::pointing_shape(h==4,bool(selectors&(1u<<31)),n);
    } else if(id==SectorStepScan::id) {
        if(h!=0)return std::unexpected(Error{ErrorCode::invalid_argument});shape=vita::detail::sector_shape(selectors,n,binding);
    }
    if(!shape)return std::unexpected(shape.error());
    if(shape->header_code!=h||shape->record_words!=r||shape->total_words!=total)return std::unexpected(Error{ErrorCode::invalid_argument});
    return shape;
}
inline Result<FieldExtent> cif1_wire_extent(FieldId id,Bytes bytes,TimestampFormatBinding binding) noexcept {
    if(id==Spectrum::id){if(bytes.size()<52)return std::unexpected(Error{ErrorCode::short_input,0,52});return FieldExtent{52,13};}
    if(id==IndexList::id) {
        if(bytes.size()<8)return std::unexpected(Error{ErrorCode::short_input,0,8});
        const auto total=load32(bytes,0),header=load32(bytes,4),width=header>>28,count=header&0xfffffu;
        auto declared=checked_multiply(total,4);if(!declared)return std::unexpected(declared.error());
        if(bytes.size()<*declared)return std::unexpected(Error{ErrorCode::short_input,0,*declared});
        if((header&0x0ff00000u)||(width!=1&&width!=2&&width!=4)||total!=2+(std::size_t{count}*width+3)/4)return std::unexpected(Error{ErrorCode::invalid_argument});
        return FieldExtent{*declared,2+std::size_t{count},count,0};
    }
    auto shape=wire_record_shape(id,bytes,binding);if(!shape)return std::unexpected(shape.error());
    return FieldExtent{shape->total_words*4,shape->work,0,shape->records};
}
inline Result<PointingReference> read_reference(Bytes bytes,std::size_t offset,bool global=false) noexcept {
    if(offset>bytes.size()||bytes.size()-offset<4)return std::unexpected(Error{ErrorCode::short_input});
    const auto raw=load32(bytes,offset);if(raw&0xfff0u)return std::unexpected(Error{ErrorCode::invalid_argument});
    PointingReference v{static_cast<std::uint16_t>(raw>>16),static_cast<std::uint8_t>((raw>>2)&3),static_cast<std::uint8_t>(raw&3)};
    auto valid=vita::detail::validate_reference(v,global);if(!valid)return std::unexpected(valid.error());return v;
}
inline std::int64_t read_signed64(Bytes bytes,std::size_t offset) noexcept {return std::bit_cast<std::int64_t>((std::uint64_t{load32(bytes,offset)}<<32)|load32(bytes,offset+4));}
inline Result<SpectrumValue> read_spectrum(Bytes bytes) noexcept {
    if(bytes.size()!=52)return std::unexpected(Error{ErrorCode::short_input});
    const auto first=load32(bytes,0);if(first&0xfff00000u)return std::unexpected(Error{ErrorCode::invalid_argument});
    SpectrumValue v;v.spectrum_type=first&255;v.averaging=(first>>8)&255;v.delta_kind=(first>>16)&15;
    v.window_type=load32(bytes,4);v.transform_points=load32(bytes,8);v.window_points=load32(bytes,12);
    v.resolution_q20=read_signed64(bytes,16);v.span_q20=read_signed64(bytes,24);v.averages=load32(bytes,32);v.weighting_raw=load32(bytes,36);
    v.f1_index=std::bit_cast<std::int32_t>(load32(bytes,40));v.f2_index=std::bit_cast<std::int32_t>(load32(bytes,44));v.delta_raw=load32(bytes,48);
    auto valid=vita::detail::validate_spectrum(v,false);if(!valid)return std::unexpected(valid.error());return v;
}
} // namespace detail
struct PointingVectorWireView {
    Bytes bytes{};
    Result<std::size_t> size() const noexcept {auto shape=detail::wire_record_shape(PointingVectorStructure::id,bytes,{});if(!shape)return std::unexpected(shape.error());if(shape->total_words*4!=bytes.size())return std::unexpected(Error{ErrorCode::invalid_argument});return shape->records;}
    Result<std::optional<PointingReference>> global() const noexcept {
        auto valid=size();if(!valid)return std::unexpected(valid.error());
        if((detail::load32(bytes,4)>>24)==4){auto ref=detail::read_reference(bytes,12,true);if(!ref)return std::unexpected(ref.error());return std::optional<PointingReference>{*ref};}
        return std::optional<PointingReference>{};
    }
    Result<PointingVectorRecord> at(std::size_t index) const noexcept {
        auto shape=detail::wire_record_shape(PointingVectorStructure::id,bytes,{});if(!shape)return std::unexpected(shape.error());
        if(shape->total_words*4!=bytes.size()||index>=shape->records)return std::unexpected(Error{ErrorCode::invalid_argument});
        std::size_t offset=(shape->header_words+index*shape->record_words)*4;PointingVectorRecord record;
        if(shape->record_words==2){auto ref=detail::read_reference(bytes,offset);if(!ref)return std::unexpected(ref.error());record.reference=*ref;offset+=4;}
        const auto angles=detail::load32(bytes,offset);record.vector={static_cast<std::uint16_t>(angles),std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(angles>>16))};return record;
    }
};
struct SectorStepScanWireView {
    Bytes bytes{};TimestampFormatBinding start_format{};
    Result<std::size_t> size() const noexcept {auto shape=detail::wire_record_shape(SectorStepScan::id,bytes,start_format);if(!shape)return std::unexpected(shape.error());if(shape->total_words*4!=bytes.size())return std::unexpected(Error{ErrorCode::invalid_argument});return shape->records;}
    Result<std::uint32_t> selectors() const noexcept {auto count=size();if(!count)return std::unexpected(count.error());return detail::load32(bytes,8);}
    Result<SectorRecord> at(std::size_t index) const noexcept {
        auto shape=detail::wire_record_shape(SectorStepScan::id,bytes,start_format);if(!shape)return std::unexpected(shape.error());
        if(shape->total_words*4!=bytes.size()||index>=shape->records)return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto mask=detail::load32(bytes,8);std::size_t offset=(shape->header_words+index*shape->record_words)*4;
        auto word=[&]() noexcept {auto result=detail::load32(bytes,offset);offset+=4;return result;};
        auto signed64=[&]() noexcept {auto result=detail::read_signed64(bytes,offset);offset+=8;return result;};
        auto pair=[&]() noexcept {auto v=word();return std::array<std::int16_t,2>{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(v)),std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(v>>16))};};
        SectorRecord v;v.sector=word();v.f1_q20=signed64();
        if(mask&(1u<<29))v.f2_q20=signed64();if(mask&(1u<<28))v.bandwidth_q20=signed64();if(mask&(1u<<27))v.step_q20=signed64();
        if(mask&(1u<<26))v.points=word();if(mask&(1u<<25))v.gain_q7=pair();if(mask&(1u<<24))v.threshold_q7=pair();if(mask&(1u<<23))v.dwell_fs=signed64();
        if(mask&(1u<<22)){SectorStartTime t{};t.tsi=start_format.tsi;t.tsf=start_format.tsf;if(t.tsi)t.seconds=word();if(t.tsf){const auto high=word();t.fractional=(std::uint64_t{high}<<32)|word();}v.start=t;}
        if(mask&(1u<<21))v.time3_fs=signed64();if(mask&(1u<<20))v.time4_fs=signed64();return v;
    }
};
namespace detail {
inline Result<void> validate_cif1_wire(FieldId id,Bytes bytes,TimestampFormatBinding binding) noexcept {
    auto extent=cif1_wire_extent(id,bytes,binding);if(!extent)return std::unexpected(extent.error());if(extent->bytes!=bytes.size())return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==IndexList::id){const auto h=load32(bytes,4);const auto used=8+(h&0xfffffu)*(h>>28);for(std::size_t i=used;i<bytes.size();++i)if(bytes[i]!=std::byte{})return std::unexpected(Error{ErrorCode::invalid_argument});}
    else if(id==PointingVectorStructure::id){PointingVectorWireView view{bytes};auto global=view.global();if(!global)return std::unexpected(global.error());for(std::size_t i=0;i<extent->records;++i){auto record=view.at(i);if(!record)return std::unexpected(record.error());}}
    else if(id==Spectrum::id){auto value=read_spectrum(bytes);if(!value)return std::unexpected(value.error());}
    return {};
}
inline void write_signed64(MutableBytes bytes,std::size_t offset,std::int64_t value) noexcept {const auto raw=std::bit_cast<std::uint64_t>(value);store32(bytes,offset,static_cast<std::uint32_t>(raw>>32));store32(bytes,offset+4,static_cast<std::uint32_t>(raw));}
inline std::uint32_t reference_bits(PointingReference v) noexcept {return (std::uint32_t{v.index}<<16)|(std::uint32_t{v.reference}<<2)|v.beam;}
inline Result<void> write_cif1(FieldId id,Bytes native,MutableBytes output,TimestampFormatBinding binding,Attribute attribute) noexcept {
    auto extent=native_structure_extent(id,native,binding,attribute);if(!extent)return std::unexpected(extent.error());if(output.size()<extent->bytes)return std::unexpected(Error{ErrorCode::short_output,0,extent->bytes});
    if(id==IndexList::id){auto v=vita::detail::native_indices(native);if(!v)return std::unexpected(v.error());store32(output,0,static_cast<std::uint32_t>(extent->bytes/4));store32(output,4,(std::uint32_t{v->entry_bytes}<<28)|static_cast<std::uint32_t>(v->size()));for(std::size_t i=8;i<extent->bytes;++i)output[i]=std::byte{};for(std::size_t i=0;i<v->size();++i){const auto n=*v->at(i);for(unsigned b=0;b<v->entry_bytes;++b)output[8+i*v->entry_bytes+b]=std::byte((n>>(8*(v->entry_bytes-1-b)))&255);}return {};}
    if(id==Spectrum::id){auto v=vita::detail::native_object<SpectrumValue>(native);if(!v)return std::unexpected(v.error());store32(output,0,(std::uint32_t{v->delta_kind}<<16)|(std::uint32_t{v->averaging}<<8)|v->spectrum_type);store32(output,4,v->window_type);store32(output,8,v->transform_points);store32(output,12,v->window_points);write_signed64(output,16,v->resolution_q20);write_signed64(output,24,v->span_q20);store32(output,32,v->averages);store32(output,36,v->weighting_raw);store32(output,40,std::bit_cast<std::uint32_t>(v->f1_index));store32(output,44,std::bit_cast<std::uint32_t>(v->f2_index));store32(output,48,v->delta_raw);return {};}
    if(id==PointingVectorStructure::id){auto v=vita::detail::native_pointing(native);if(!v)return std::unexpected(v.error());const auto shape=*vita::detail::pointing_shape(bool(v->global),v->record_reference,v->records.size());store32(output,0,static_cast<std::uint32_t>(shape.total_words));store32(output,4,static_cast<std::uint32_t>((shape.header_code<<24)|(shape.record_words<<12)|shape.records));store32(output,8,0x40000000u|(v->record_reference?0x80000000u:0));std::size_t offset=12;if(v->global){store32(output,offset,reference_bits(*v->global));offset+=4;}for(std::size_t i=0;i<v->records.size();++i){const auto r=*v->records.at(i);if(r.reference){store32(output,offset,reference_bits(*r.reference));offset+=4;}store32(output,offset,(std::uint32_t{std::bit_cast<std::uint16_t>(r.vector.elevation_q7)}<<16)|r.vector.azimuth_q7);offset+=4;}return {};}
    auto v=vita::detail::native_sectors(native);if(!v)return std::unexpected(v.error());const auto shape=*vita::detail::sector_shape(v->selectors,v->records.size(),v->start_format);store32(output,0,static_cast<std::uint32_t>(shape.total_words));store32(output,4,static_cast<std::uint32_t>((shape.header_code<<24)|(shape.record_words<<12)|shape.records));store32(output,8,v->selectors);std::size_t offset=12;
    auto word=[&](std::uint32_t n) noexcept {store32(output,offset,n);offset+=4;};auto signed64=[&](std::int64_t n) noexcept {write_signed64(output,offset,n);offset+=8;};auto pair=[&](std::array<std::int16_t,2> n) noexcept {word((std::uint32_t{std::bit_cast<std::uint16_t>(n[1])}<<16)|std::bit_cast<std::uint16_t>(n[0]));};
    for(std::size_t i=0;i<v->records.size();++i){const auto r=*v->records.at(i);word(r.sector);signed64(r.f1_q20);if(r.f2_q20)signed64(*r.f2_q20);if(r.bandwidth_q20)signed64(*r.bandwidth_q20);if(r.step_q20)signed64(*r.step_q20);if(r.points)word(*r.points);if(r.gain_q7)pair(*r.gain_q7);if(r.threshold_q7)pair(*r.threshold_q7);if(r.dwell_fs)signed64(*r.dwell_fs);if(r.start){if(r.start->tsi)word(r.start->seconds);if(r.start->tsf){word(static_cast<std::uint32_t>(r.start->fractional>>32));word(static_cast<std::uint32_t>(r.start->fractional));}}if(r.time3_fs)signed64(*r.time3_fs);if(r.time4_fs)signed64(*r.time4_fs);}
    return {};
}
} // namespace detail
} // namespace vita::codec
