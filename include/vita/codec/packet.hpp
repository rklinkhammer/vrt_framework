#pragma once
#include <vita/codec/scalar.hpp>
#include <vita/codec/structured.hpp>
namespace vita::codec {
struct RequestContext { std::uint32_t cam=0; };
using DecodeLimits = TraversalLimits;
struct DecodeOptions { std::optional<RequestContext> request{};std::optional<ClassId> expected_class{}; DecodeLimits limits{}; };
enum class DiagnosticGroup { none,warning,error };
namespace detail {
inline Result<void> validate_wire_value(FieldId id,Bytes bytes) noexcept {
    if((id==AirTemperature::id||id==SeaGroundTemperature::id||id==Humidity::id||id==SeaSwellState::id||id==TroposphericState::id) && (detail::load32(bytes,0)&0xffff0000u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==BarometricPressure::id && (detail::load32(bytes,0)&0xfffe0000u))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==TimestampDetails::id && (detail::load32(bytes,0)&0x00f80000u))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(cif2_generic16_field(id) && (detail::load32(bytes,0)&0xffff0000u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==Bind::id && (detail::load32(bytes,0)&0xfffffffeu))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    // Repeated Rules9.8.7-1/2/3 select eleven code bits, over the conflicting figure.
    if(id==CountryCode::id && (detail::load32(bytes,0)&0xffff7800u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==EmsDeviceClass::id && ((detail::load32(bytes,0)&0xffff0000u) || (detail::load32(bytes,0)&0xc000u)==0xc000u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if((id==PhaseOffset::id || id==CompressionPoint::id || id==SpatialScanType::id || id==HealthStatus::id) &&
       (detail::load32(bytes,0)&0xffff0000u))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==SpatialReferenceType::id && ((detail::load32(bytes,0)&0x0000fff0u) || (detail::load32(bytes,0)&3u)==3u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==BufferSize::id && (detail::load32(bytes,4)&0xffff0000u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if((id==ReferenceLevel::id || id==Temperature::id) && (detail::load32(bytes,0)&0xffff0000u))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==DeviceIdentifier::id && ((detail::load32(bytes,0)&0xff000000u) || (detail::load32(bytes,4)&0xffff0000u)))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==StateEvent::id && (detail::load32(bytes,0)&0x00f00f00u))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==DataPayloadFormat::id) {
        const auto word=detail::load32(bytes,0),code=(word>>24)&31,kind=(word>>29)&3;
        const auto fraction=(word>>12)&15,packing=((word>>6)&63)+1,bits=(word&63)+1;
        if(kind==3||(code>=8&&code<=12)||code>=24 || packing<bits ||
            ((code!=7&&code!=23)&&fraction) || fraction>=bits ||
            (code==13&&bits!=16)||(code==14&&bits!=32)||(code==15&&bits!=64))
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    return {};
}
} // namespace detail
inline Result<void> validate_wire_attribute(FieldId id,Attribute attribute,Bytes bytes,TimestampFormatBinding binding={}) noexcept {
    if(!base_attribute(attribute)) {
        auto value=detail::read_attribute_scalar(*descriptor(id),attribute,bytes);if(!value)return std::unexpected(value.error());return {};
    }
    return structured_field(id)?detail::validate_structure_wire(id,bytes,binding):detail::validate_wire_value(id,bytes);
}
struct FieldView {
    FieldId id{};Attribute attribute=Attribute::current;BodyKind kind=BodyKind::values;DiagnosticGroup group=DiagnosticGroup::none;
    TimestampFormatBinding timestamp_format{};Bytes bytes{};
    constexpr FieldView() noexcept=default;
    constexpr FieldView(FieldId field,Attribute attr,BodyKind body,DiagnosticGroup diagnostic,Bytes data,TimestampFormatBinding binding={}) noexcept
        :id(field),attribute(attr),kind(body),group(diagnostic),timestamp_format(binding),bytes(data) {}
    Result<IndexListView> indices() const noexcept {
        if(kind!=BodyKind::values||!base_attribute(attribute)||id!=IndexList::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto valid=detail::validate_cif1_wire(id,bytes,timestamp_format);if(!valid)return std::unexpected(valid.error());
        const auto header=detail::load32(bytes,4);return IndexListView{static_cast<std::uint8_t>(header>>28),header&0xfffffu,bytes.subspan(8),true};
    }
    Result<PointingVectorWireView> pointing_vectors() const noexcept {
        if(kind!=BodyKind::values||!base_attribute(attribute)||id!=PointingVectorStructure::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto valid=detail::validate_cif1_wire(id,bytes,timestamp_format);if(!valid)return std::unexpected(valid.error());return PointingVectorWireView{bytes};
    }
    Result<SpectrumValue> spectrum() const noexcept {
        if(kind!=BodyKind::values||!base_attribute(attribute)||id!=Spectrum::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::read_spectrum(bytes);
    }
    Result<SectorStepScanWireView> sectors() const noexcept {
        if(kind!=BodyKind::values||!base_attribute(attribute)||id!=SectorStepScan::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto valid=detail::validate_cif1_wire(id,bytes,timestamp_format);if(!valid)return std::unexpected(valid.error());return SectorStepScanWireView{bytes,timestamp_format};
    }
    Result<StateDurationValue> duration() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || !temporal_duration_field(id))return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_duration(bytes,timestamp_format);
    }
    Result<SemanticValue> value() const noexcept {
        if(kind!=BodyKind::values||static_cast<unsigned>(attribute)>=13)return std::unexpected(Error{ErrorCode::invalid_state});
        const auto* d=descriptor(id);if(!d)return std::unexpected(Error{ErrorCode::unsupported_layout});
        if(base_attribute(attribute)&&structured_field(id))return std::unexpected(Error{ErrorCode::unsupported_capability});
        return detail::read_attribute_scalar(*d,attribute,bytes);
    }
    Result<UuidValue> uuid() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || (id!=ControlleeUUID::id && id!=ControllerUUID::id))return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_uuid(bytes);
    }
    Result<GeolocationValue> geolocation() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || (id!=FormattedGPS::id && id!=FormattedINS::id))return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_geolocation(bytes);
    }
    Result<EphemerisValue> ephemeris() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || (id!=ECEFEphemeris::id && id!=RelativeEphemeris::id))return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_ephemeris(bytes);
    }
    Result<GpsAsciiView> gps_ascii() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || id!=GPSASCII::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_ascii(bytes);
    }
    Result<AssociationListsView> associations() const noexcept {
        if(kind!=BodyKind::values || !base_attribute(attribute) || id!=ContextAssociationLists::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        return detail::wire_associations(bytes);
    }
    template<class Builder> Result<void> materialize_into(Builder& builder,SentenceValidator validator={}) const noexcept {
        if(kind!=BodyKind::values)return std::unexpected(Error{ErrorCode::unsupported_capability});
        auto apply=[&]<class Field>(typename Field::value_type value) noexcept -> Result<void> {
            if(attribute==Attribute::current&&!builder.has_attribute(Field::id,attribute))return builder.template set<Field>(value);
            return builder.template set_attribute<Field>(attribute,value);
        };
        if(!base_attribute(attribute)) {auto scalar=value();if(!scalar)return std::unexpected(scalar.error());return builder.set_attribute_input({id,attribute,InputValue{*scalar}});}
        if(id==IndexList::id) {
            auto view=indices();if(!view)return std::unexpected(view.error());std::array<std::uint32_t,1024> values{};
            if(view->size()>values.size())return std::unexpected(Error{ErrorCode::resource_limit});
            for(std::size_t i=0;i<view->size();++i){auto v=view->at(i);if(!v)return std::unexpected(v.error());values[i]=*v;}
            return apply.template operator()<IndexList>({view->entry_bytes,{values.data(),view->size()}});
        }
        if(id==PointingVectorStructure::id) {
            auto view=pointing_vectors();if(!view)return std::unexpected(view.error());auto count=view->size();if(!count)return std::unexpected(count.error());
            std::array<PointingVectorRecord,256> records{};if(*count>records.size())return std::unexpected(Error{ErrorCode::resource_limit});
            for(std::size_t i=0;i<*count;++i){auto v=view->at(i);if(!v)return std::unexpected(v.error());records[i]=*v;}
            auto global=view->global();if(!global)return std::unexpected(global.error());
            return apply.template operator()<PointingVectorStructure>({*global,bool(detail::load32(bytes,8)&(1u<<31)),{records.data(),*count}});
        }
        if(id==Spectrum::id){auto v=spectrum();if(!v)return std::unexpected(v.error());return apply.template operator()<Spectrum>(*v);}
        if(id==SectorStepScan::id) {
            auto view=sectors();if(!view)return std::unexpected(view.error());auto count=view->size();if(!count)return std::unexpected(count.error());
            std::array<SectorRecord,256> records{};if(*count>records.size())return std::unexpected(Error{ErrorCode::resource_limit});
            for(std::size_t i=0;i<*count;++i){auto v=view->at(i);if(!v)return std::unexpected(v.error());records[i]=*v;}
            auto selectors=view->selectors();if(!selectors)return std::unexpected(selectors.error());
            return apply.template operator()<SectorStepScan>({*selectors,timestamp_format,{records.data(),*count}});
        }
        if(temporal_duration_field(id)) {
            auto value=duration();if(!value)return std::unexpected(value.error());
            return id==Age::id?apply.template operator()<Age>(*value):apply.template operator()<ShelfLife>(*value);
        }
        if(id==ControlleeUUID::id || id==ControllerUUID::id) {
            auto value=uuid();if(!value)return std::unexpected(value.error());
            return id==ControlleeUUID::id?apply.template operator()<ControlleeUUID>(*value):apply.template operator()<ControllerUUID>(*value);
        }
        if(id==FormattedGPS::id || id==FormattedINS::id) {
            auto value=geolocation();if(!value)return std::unexpected(value.error());
            return id==FormattedGPS::id?apply.template operator()<FormattedGPS>(*value):apply.template operator()<FormattedINS>(*value);
        }
        if(id==ECEFEphemeris::id || id==RelativeEphemeris::id) {
            auto value=ephemeris();if(!value)return std::unexpected(value.error());
            return id==ECEFEphemeris::id?apply.template operator()<ECEFEphemeris>(*value):apply.template operator()<RelativeEphemeris>(*value);
        }
        if(id==GPSASCII::id) {
            auto value=gps_ascii();if(!value)return std::unexpected(value.error());
            return apply.template operator()<GPSASCII>(GpsAsciiInput{value->oui,value->text,validator});
        }
        if(id==ContextAssociationLists::id) {
            auto value=associations();if(!value)return std::unexpected(value.error());
            const std::array lists{value->source,value->system,value->vector,value->asynchronous,value->tags};
            std::array<std::uint32_t,1024> storage{};std::array<std::span<const std::uint32_t>,5> copied{};std::size_t used=0;
            for(unsigned i=0;i<5;++i) {
                if(lists[i].size()>storage.size()-used)return std::unexpected(Error{ErrorCode::resource_limit});
                const auto begin=used;
                for(std::size_t j=0;j<lists[i].size();++j)storage[used++]=*lists[i].at(j);
                copied[i]={storage.data()+begin,used-begin};
            }
            return apply.template operator()<ContextAssociationLists>(AssociationListsInput{copied[0],copied[1],copied[2],copied[3],copied[4],value->tags_present});
        }
        auto scalar=value();if(!scalar)return std::unexpected(scalar.error());if(attribute==Attribute::current&&!builder.has_attribute(id,attribute))return builder.set_value(id,*scalar);return builder.set_attribute_input({id,attribute,InputValue{*scalar}});
    }
    Result<std::uint32_t> diagnostic() const noexcept {
        if(kind!=BodyKind::diagnostics||bytes.size()!=4)return std::unexpected(Error{ErrorCode::invalid_state});return detail::load32(bytes,0);
    }
};
template<std::size_t ViewCapacity> struct BasicPacketView {
    EnvelopeView envelope;
    FixedVector<FieldView,ViewCapacity> fields;
    BodyKind body_kind=BodyKind::values;
    bool opaque=false,change=false;
    // Diagnostic group placement depends on the correlated request detail mask (I11).
    bool requires_request_context=false;
};
using PacketView=BasicPacketView<64>;
inline SemanticValue placeholder(FieldId id) noexcept {
    const auto* d=descriptor(id);
    if(!d || structured_field(id))return std::uint32_t{};
    std::array<std::byte,8> zero{};
    return *detail::read_scalar(*d,Bytes{zero}.first(d->words*4));
}
template<std::size_t Capacity> struct IndicatorPlan {
    struct Selected { FieldId id{}; };
    LayoutContext context{};
    std::array<Selected,Capacity> selected{};
    std::size_t count=0,bytes=0;bool change=false;
    constexpr const LayoutContext& layout() const noexcept{return context;}
    constexpr std::span<const Selected> fields() const noexcept{return {selected.data(),count};}
    constexpr std::uint64_t generation() const noexcept{return 0;}
};
template<std::size_t C> constexpr std::uint64_t layout_signature(const IndicatorPlan<C>& plan) noexcept {
    std::uint64_t result=14695981039346656037ull;for(auto word:plan.context.cif)result=(result^word)*1099511628211ull;return result;
}
using IndicatorView=IndicatorPlan<16>;
template<std::size_t Capacity=16> inline Result<IndicatorPlan<Capacity>> parse_indicators(Bytes data,std::size_t offset,bool allow_change,std::size_t* aggregate=nullptr) noexcept {
    const auto start=offset;
    auto take=[&]() noexcept -> Result<std::uint32_t>{if(offset>data.size()||data.size()-offset<4)return std::unexpected(Error{ErrorCode::short_input,offset,offset+4});auto v=detail::load32(data,offset);offset+=4;return v;};
    auto first=take();if(!first)return std::unexpected(first.error());
    if((*first&0x71u)||(!allow_change&&(*first&(1u<<31))))return std::unexpected(Error{ErrorCode::invalid_argument,start});
    IndicatorPlan<Capacity> result{};auto& cif=result.context.cif;cif[0]=*first;
    for(unsigned i=1;i<8;++i)if(*first&(1u<<i)){auto v=take();if(!v)return std::unexpected(v.error());cif[i]=*v;}
    result.bytes=offset-start;result.change=*first&(1u<<31);
    if(*first&(1u<<7)) {
        if(cif[7]==0||(cif[7]&~all_attributes))return std::unexpected(Error{ErrorCode::invalid_argument,start});
        result.context.attributes=cif[7];
    }
    for(unsigned i=0;i<4;++i) {
        const auto bits=i==0?cif[0]&0x7fffff00u:cif[i];
        for(int bit=31;bit>=0;--bit)if(bits&(std::uint32_t{1}<<bit)) {
            const FieldId id{static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(bit)};
            if(!descriptor(id))return std::unexpected(Error{ErrorCode::unsupported_layout,start});
            if(result.count==Capacity)return std::unexpected(Error{ErrorCode::resource_limit,start});
            result.selected[result.count++].id=id;
        }
    }
    for(unsigned i=1;i<4;++i)if((*first&(1u<<i))&&cif[i]==0)return std::unexpected(Error{ErrorCode::unsupported_layout,start});
    if(aggregate) {if(*aggregate>Capacity||result.count>Capacity-*aggregate)return std::unexpected(Error{ErrorCode::resource_limit,start});*aggregate+=result.count;}
    return result;
}
inline Result<void> validate_diagnostic_bits(std::uint32_t bits,bool cancel) noexcept {
    // §8.4.1.2.1: reserved 18..13 and bit0. User bits12..1 remain class-defined.
    if(bits&0x0007e001u)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(cancel && (bits&0x1ff80000u))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
template<BodyKind K,std::size_t V,std::size_t F> inline Result<std::size_t> append_fields(BasicPacketView<V>& packet,const IndicatorPlan<F>& indicators,std::size_t values_offset,
        DiagnosticGroup group=DiagnosticGroup::none,DecodeLimits limits={},std::size_t* shared_work=nullptr) noexcept {
    const auto& layout=indicators;
    const auto payload=packet.envelope.payload;
    const TimestampFormatBinding binding{static_cast<std::uint8_t>(packet.envelope.envelope.timestamp.tsi),static_cast<std::uint8_t>(packet.envelope.envelope.timestamp.tsf),true};
    const auto own_header=indicators.bytes;
    auto wire_offset=[&](std::size_t offset) noexcept { return checked_add(values_offset,offset-own_header); };
    auto provider=[&](const auto& field,Attribute attribute,std::size_t offset) noexcept -> Result<FieldExtent> {
        auto at=wire_offset(offset);if(!at)return std::unexpected(at.error());
        if(*at>payload.size())return std::unexpected(Error{ErrorCode::short_input,*at});
        if(base_attribute(attribute)&&structured_field(field.id))return detail::wire_structure_extent(field.id,payload.subspan(*at),binding);
        const auto size=base_attribute(attribute)?descriptor(field.id)->words*4:4;
        if(size>payload.size()-*at)return std::unexpected(Error{ErrorCode::short_input,*at,*at+size});
        return FieldExtent{size,1};
    };
    auto measured=walk_field_layout<K>(layout,provider,[&](LayoutElement e) noexcept -> Result<void> {
        auto offset=wire_offset(e.offset);if(!offset)return std::unexpected(offset.error());
        if(*offset>payload.size() || e.bytes>payload.size()-*offset)return std::unexpected(Error{ErrorCode::short_input,*offset,*offset+e.bytes});
        if constexpr(K==BodyKind::diagnostics){auto valid=validate_diagnostic_bits(detail::load32(payload,*offset),packet.envelope.envelope.cancel);if(!valid)return valid;}
        if constexpr(K==BodyKind::values) {
            auto fieldbytes=payload.subspan(*offset,e.bytes);
            auto valid=validate_wire_attribute(e.field,e.attribute,fieldbytes,binding);
            if(!valid)return valid;
        }
        if(packet.fields.full())return std::unexpected(Error{ErrorCode::resource_limit});
        return packet.fields.push_back(FieldView{e.field,e.attribute,K,group,payload.subspan(*offset,e.bytes),binding});
    },0,limits,shared_work);
    if(!measured)return std::unexpected(measured.error());
    return measured->bytes-own_header;
}
template<std::size_t FieldCapacity,std::size_t ViewCapacity> inline Result<BasicPacketView<ViewCapacity>> decode_packet_bounded(Bytes wire,DecodeOptions options={}) noexcept {
    auto envelope=decode_envelope(wire);if(!envelope)return std::unexpected(envelope.error());
    if(options.expected_class && envelope->envelope.class_id!=options.expected_class)return std::unexpected(Error{ErrorCode::unsupported_capability});
    std::size_t work=0,selected_fields=0;
    BasicPacketView<ViewCapacity> result{*envelope,{}};const auto& e=envelope->envelope;const auto payload=envelope->payload;
    if(is_data(e.type)||is_extension(e.type)){result.opaque=true;return result;}
    if(e.type==PacketType::context) {
        auto indicators=parse_indicators<FieldCapacity>(payload,0,true,&selected_fields);if(!indicators)return std::unexpected(indicators.error());result.change=indicators->change;
        auto size=append_fields<BodyKind::values>(result,*indicators,indicators->bytes,DiagnosticGroup::none,options.limits,&work);if(!size)return std::unexpected(size.error());
        if(indicators->bytes+*size!=payload.size())return std::unexpected(Error{ErrorCode::invalid_argument,indicators->bytes+*size});return result;
    }
    const auto cam=e.command->cam;
    if(!e.ack || (cam&(1u<<18))) {
        const bool selectors=!e.ack && (e.cancel||((cam>>23)&3)==0);
        // Permission 9.1.1-1 permits ordinary Control use (I4); cancellation and Acks remain separate.
        const bool allow_change=!e.ack && !e.cancel;
        auto indicators=parse_indicators<FieldCapacity>(payload,0,allow_change,&selected_fields);if(!indicators)return std::unexpected(indicators.error());
        result.change=indicators->change;
        std::size_t body=0;
        if(selectors) {
            result.body_kind=BodyKind::selectors;const auto& snapshot=*indicators;
            auto selected=walk_field_layout<BodyKind::selectors,true>(snapshot,
                [](const auto&,Attribute,std::size_t) noexcept -> Result<FieldExtent>{return FieldExtent{};},
                [&](LayoutElement element) noexcept -> Result<void> {
                    if(result.fields.full())return std::unexpected(Error{ErrorCode::resource_limit});
                    return result.fields.push_back(FieldView{element.field,element.attribute,BodyKind::selectors,DiagnosticGroup::none,{}});
                },0,options.limits,&work);
            if(!selected)return std::unexpected(selected.error());
        }else{auto size=append_fields<BodyKind::values>(result,*indicators,indicators->bytes,DiagnosticGroup::none,options.limits,&work);if(!size)return std::unexpected(size.error());body=*size;}
        if(indicators->bytes+body!=payload.size())return std::unexpected(Error{ErrorCode::invalid_argument,indicators->bytes+body});return result;
    }
    result.body_kind=BodyKind::diagnostics;
    const bool warning=cam&(1u<<17),error=cam&(1u<<16);
    if((warning||error)&&!options.request) {
        // Envelope/CAM framing is checked, but never guess a diagnostic body's group identity.
        result.opaque=true;result.requires_request_context=true;return result;
    }
    const bool warning_body=warning&&options.request&&(options.request->cam&(1u<<17));
    const bool error_body=error&&options.request&&(options.request->cam&(1u<<16));
    std::optional<IndicatorPlan<FieldCapacity>> warnings,errors;std::size_t offset=0;
    if(warning_body){auto v=parse_indicators<FieldCapacity>(payload,offset,false,&selected_fields);if(!v)return std::unexpected(v.error());warnings=*v;offset+=v->bytes;}
    if(error_body){auto v=parse_indicators<FieldCapacity>(payload,offset,false,&selected_fields);if(!v)return std::unexpected(v.error());errors=*v;offset+=v->bytes;}
    if(warnings){auto n=append_fields<BodyKind::diagnostics>(result,*warnings,offset,DiagnosticGroup::warning,options.limits,&work);if(!n)return std::unexpected(n.error());offset+=*n;}
    if(errors){auto n=append_fields<BodyKind::diagnostics>(result,*errors,offset,DiagnosticGroup::error,options.limits,&work);if(!n)return std::unexpected(n.error());offset+=*n;}
    if(offset!=payload.size())return std::unexpected(Error{ErrorCode::invalid_argument,offset});return result;
}
inline Result<PacketView> decode_packet(Bytes wire,DecodeOptions options={}) noexcept {return decode_packet_bounded<16,64>(wire,options);}
// Delivers no application callbacks until the complete packet has validated.
template<class Visitor> inline Result<void> decode_and_visit(Bytes wire,DecodeOptions options,Visitor&& visitor) noexcept {
    auto parsed=decode_packet(wire,options);if(!parsed)return std::unexpected(parsed.error());
    for(std::size_t i=0;i<parsed->fields.size();++i){auto r=visitor(parsed->fields[i]);if(!r)return r;}return {};
}
template<std::size_t FieldCapacity,std::size_t ViewCapacity,class Visitor> inline Result<void> decode_and_visit_bounded(Bytes wire,DecodeOptions options,Visitor&& visitor) noexcept {
    auto parsed=decode_packet_bounded<FieldCapacity,ViewCapacity>(wire,options);if(!parsed)return std::unexpected(parsed.error());
    for(std::size_t i=0;i<parsed->fields.size();++i){auto result=visitor(parsed->fields[i]);if(!result)return result;}return {};
}
namespace detail {
template<BodyKind K,std::size_t N,std::size_t A> inline void write_indicators(const PacketSnapshot<K,N,A>& packet,MutableBytes output,bool change=false) noexcept {
    const auto& cif=packet.layout().cif;std::size_t offset=0;detail::store32(output,offset,cif[0]|(change?1u<<31:0));offset+=4;
    for(unsigned i=1;i<8;++i)if(cif[0]&(1u<<i)){detail::store32(output,offset,cif[i]);offset+=4;}
}
template<BodyKind K,std::size_t N,std::size_t A> inline Result<void> write_values(const PacketSnapshot<K,N,A>& packet,MutableBytes output,std::size_t values_start,std::size_t own_header) noexcept {
    auto walked=walk_layout(packet,[&](LayoutElement e) noexcept -> Result<void>{
        const auto offset=values_start+e.offset-own_header;
        const FieldEntry* entry=nullptr;for(const auto& f:packet.fields())if(f.id==e.field){entry=&f;break;}
        if(!entry)return std::unexpected(Error{ErrorCode::invalid_state});
        if constexpr(K==BodyKind::diagnostics)detail::store32(output,offset,entry->diagnostic);
        else if constexpr(K==BodyKind::values){const auto& value=*entry->values[static_cast<unsigned>(e.attribute)];
            if(base_attribute(e.attribute)&&structured_field(e.field)) {
                auto native=packet.native_value(value);if(!native)return std::unexpected(native.error());
                auto written=detail::write_structure(e.field,*native,output.subspan(offset,e.bytes),packet.layout().timestamp_format,e.attribute);if(!written)return written;
            } else detail::write_attribute_scalar(*descriptor(e.field),e.attribute,value,output,offset);
        }return {};
    });
    if(!walked)return std::unexpected(walked.error());return {};
}
} // namespace detail
template<BodyKind K,std::size_t N,std::size_t A> inline Result<std::size_t> encode_packet(const Envelope& e,const PacketSnapshot<K,N,A>& snapshot,MutableBytes output,bool change=false) noexcept {
    if(is_data(e.type)||is_extension(e.type)||e.trailer)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(change && e.type!=PacketType::context && !(e.type==PacketType::command && !e.ack && !e.cancel))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.type==PacketType::context) {if constexpr(K!=BodyKind::values)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(snapshot.layout().subtype!=PacketSubtype::context)return std::unexpected(Error{ErrorCode::invalid_argument});}
    else {
        if(!e.command)return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto cam=e.command->cam;const bool selectors=!e.ack&&(e.cancel||((cam>>23)&3)==0);
        const auto required=selectors?BodyKind::selectors:BodyKind::values;
        if(required!=K || (e.ack&&!(cam&(1u<<18))))return std::unexpected(Error{ErrorCode::invalid_argument});
        if(snapshot.layout().action!=((cam>>23)&3))return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto expected=e.ack?PacketSubtype::state_ack:e.cancel?PacketSubtype::cancel:selectors?PacketSubtype::query:PacketSubtype::control;
        if(snapshot.layout().subtype!=expected)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if(snapshot.layout().packet_class && (!e.class_id || snapshot.layout().packet_class!=e.class_id->packet_class))return std::unexpected(Error{ErrorCode::invalid_argument});
    if constexpr(K==BodyKind::values)for(const auto& field:snapshot.fields())if(temporal_duration_field(field.id)||field.id==SectorStepScan::id) {
        for(unsigned a=0;a<11;++a)if(field.values[a]) {
            auto bytes=snapshot.native_value(*field.values[a]);if(!bytes)return std::unexpected(bytes.error());
            auto required=native_timestamp_binding(field.id,*bytes);if(!required)return std::unexpected(required.error());
            if(*required) {
                const auto binding=snapshot.layout().timestamp_format;
                if(!binding.bound)return std::unexpected(Error{ErrorCode::unsupported_capability});
                if(binding.tsi!=static_cast<unsigned>(e.timestamp.tsi)||binding.tsf!=static_cast<unsigned>(e.timestamp.tsf))return std::unexpected(Error{ErrorCode::invalid_argument});
            }
        }
    }
    auto body=measure(snapshot);if(!body)return std::unexpected(body.error());
    if constexpr(K==BodyKind::values) {
        std::array<std::byte,8> scratch{};
        for(const auto& field:snapshot.fields())for(unsigned a=0;a<13;++a)if(field.values[a]) {
            const auto attribute=static_cast<Attribute>(a);
            if(base_attribute(attribute)&&structured_field(field.id))continue;
            detail::write_attribute_scalar(*descriptor(field.id),attribute,*field.values[a],scratch);
            auto bytes=Bytes{scratch}.first(base_attribute(attribute)?descriptor(field.id)->words*4:4);
            auto valid=validate_wire_attribute(field.id,attribute,bytes);if(!valid)return std::unexpected(valid.error());
        }
    }
    auto total=measure_envelope(e,body->bytes);if(!total)return std::unexpected(total.error());
    if(output.size()<*total)return std::unexpected(Error{ErrorCode::short_output,0,*total});
    auto payload=output.subspan(*total-body->bytes,body->bytes);detail::write_indicators(snapshot,payload,change);
    auto written=detail::write_values(snapshot,payload,0,0);if(!written)return std::unexpected(written.error());
    return encode_envelope(e,payload,std::nullopt,output);
}
template<std::size_t N> inline Result<std::size_t> encode_diagnostic(const Envelope& e,const PacketSnapshot<BodyKind::diagnostics,N>& warnings,const PacketSnapshot<BodyKind::diagnostics,N>& errors,RequestContext original,MutableBytes output) noexcept {
    if(e.type!=PacketType::command||!e.ack||!e.command||!(e.command->cam&((1u<<20)|(1u<<19))))return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto cam=e.command->cam;const bool w=(cam&(1u<<17))&&(original.cam&(1u<<17)),r=(cam&(1u<<16))&&(original.cam&(1u<<16));
    if((!w&&!warnings.fields().empty())||(!r&&!errors.fields().empty()))return std::unexpected(Error{ErrorCode::invalid_argument});
    auto wm=measure(warnings),em=measure(errors);if(!wm)return std::unexpected(wm.error());if(!em)return std::unexpected(em.error());
    auto indicator_size=[](const auto& p){std::size_t n=4;for(unsigned i=1;i<8;++i)if(p.layout().cif[0]&(1u<<i))n+=4;return n;};
    const auto wh=indicator_size(warnings),eh=indicator_size(errors);const std::size_t body=(w?wm->bytes:0)+(r?em->bytes:0);
    for(const auto* p:{&warnings,&errors})for(const auto& f:p->fields()){auto valid=validate_diagnostic_bits(f.diagnostic,e.cancel);if(!valid)return std::unexpected(valid.error());}
    auto total=measure_envelope(e,body);if(!total)return std::unexpected(total.error());if(output.size()<*total)return std::unexpected(Error{ErrorCode::short_output,0,*total});
    auto payload=output.subspan(*total-body,body);std::size_t offset=0;
    if(w){detail::write_indicators(warnings,payload.subspan(offset));offset+=wh;}if(r){detail::write_indicators(errors,payload.subspan(offset));offset+=eh;}
    if(w){auto v=detail::write_values(warnings,payload,offset,wh);if(!v)return std::unexpected(v.error());offset+=wm->bytes-wh;}
    if(r){auto v=detail::write_values(errors,payload,offset,eh);if(!v)return std::unexpected(v.error());}
    return encode_envelope(e,payload,std::nullopt,output);
}
} // namespace vita::codec
