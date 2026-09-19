#pragma once
#include <vita/codec/wire.hpp>
namespace vita::codec {
struct RequestContext { std::uint32_t cam=0; };
struct DecodeOptions { std::optional<RequestContext> request{};std::optional<ClassId> expected_class{}; };
enum class DiagnosticGroup { none,warning,error };
namespace detail {
inline Result<void> validate_wire_value(FieldId id,Bytes bytes) noexcept {
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
struct FieldView {
    FieldId id;Attribute attribute;BodyKind kind;DiagnosticGroup group;Bytes bytes;
    Result<SemanticValue> value() const noexcept {
        if(kind!=BodyKind::values)return std::unexpected(Error{ErrorCode::invalid_state});
        const auto* d=descriptor(id);if(!d)return std::unexpected(Error{ErrorCode::unsupported_layout});
        if(bytes.size()!=d->words*4)return std::unexpected(Error{ErrorCode::short_input});
        if(id==SampleRate::id)return SemanticValue{Hertz{std::bit_cast<std::int64_t>((std::uint64_t{detail::load32(bytes,0)}<<32)|detail::load32(bytes,4))}};
        if(id==DataPayloadFormat::id)return SemanticValue{PayloadFormat{(std::uint64_t{detail::load32(bytes,0)}<<32)|detail::load32(bytes,4)}};
        return SemanticValue{detail::load32(bytes,0)};
    }
    Result<std::uint32_t> diagnostic() const noexcept {
        if(kind!=BodyKind::diagnostics||bytes.size()!=4)return std::unexpected(Error{ErrorCode::invalid_state});return detail::load32(bytes,0);
    }
};
struct PacketView {
    EnvelopeView envelope;
    FixedVector<FieldView,64> fields;
    BodyKind body_kind=BodyKind::values;
    bool opaque=false,change=false;
    // Diagnostic group placement depends on the correlated request detail mask (I11).
    bool requires_request_context=false;
};
inline SemanticValue placeholder(FieldId id) noexcept {
    if(id==SampleRate::id)return Hertz{};if(id==DataPayloadFormat::id)return PayloadFormat{};return std::uint32_t{};
}
struct IndicatorView { QueryPacket selectors;std::size_t bytes=0;bool change=false; };
inline Result<IndicatorView> parse_indicators(Bytes data,std::size_t offset,bool allow_change) noexcept {
    const auto start=offset;
    auto take=[&]() noexcept -> Result<std::uint32_t>{if(offset>data.size()||data.size()-offset<4)return std::unexpected(Error{ErrorCode::short_input,offset,offset+4});auto v=detail::load32(data,offset);offset+=4;return v;};
    auto first=take();if(!first)return std::unexpected(first.error());
    if((*first&0x71u)||(!allow_change&&(*first&(1u<<31))))return std::unexpected(Error{ErrorCode::invalid_argument,start});
    std::array<std::uint32_t,8> cif{};cif[0]=*first;
    for(unsigned i=1;i<8;++i)if(*first&(1u<<i)){auto v=take();if(!v)return std::unexpected(v.error());cif[i]=*v;}
    IndicatorView result{};result.bytes=offset-start;result.change=*first&(1u<<31);
    for(unsigned i=0;i<4;++i) {
        const auto bits=i==0?cif[0]&0x7fffff00u:cif[i];
        for(int bit=31;bit>=0;--bit)if(bits&(std::uint32_t{1}<<bit)) {
            const FieldId id{static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(bit)};
            if(!descriptor(id))return std::unexpected(Error{ErrorCode::unsupported_layout,start});
            auto selected=result.selectors.select(id);if(!selected)return std::unexpected(selected.error());
        }
    }
    if(*first&(1u<<7)) {
        if(cif[7]==0)return std::unexpected(Error{ErrorCode::invalid_argument,start});
        auto attrs=result.selectors.with_attributes(cif[7]);if(!attrs)return std::unexpected(attrs.error());
    }
    // Empty extra CIF words carry no known fields and are noncanonical here.
    for(unsigned i=1;i<4;++i)if((*first&(1u<<i)) && cif[i]==0)return std::unexpected(Error{ErrorCode::unsupported_layout,start});
    return result;
}
template<BodyKind K> inline Result<PacketSnapshot<K>> layout_from(const IndicatorView& indicators) noexcept {
    PacketBuilder<K> builder;
    const auto selected=indicators.selectors.freeze();
    for(const auto& e:selected.fields()) {
        Result<void> added;
        if constexpr(K==BodyKind::values)added=builder.set_value(e.id,placeholder(e.id));
        else if constexpr(K==BodyKind::diagnostics)added=builder.diagnostic(e.id,0);
        else added=builder.select(e.id);
        if(!added)return std::unexpected(added.error());
    }
    const auto mask=selected.layout().attributes;
    if(mask) {
        if constexpr(K==BodyKind::values) {
            std::array<AttributeValue,48> supplied{};std::size_t count=0;
            for(const auto& e:selected.fields())for(unsigned a=0;a<13;++a)if(mask&attribute_bit(static_cast<Attribute>(a))) {
                if(count==supplied.size())return std::unexpected(Error{ErrorCode::resource_limit});
                supplied[count++]={e.id,static_cast<Attribute>(a),placeholder(e.id)};
            }
            auto applied=builder.with_attributes(mask,{supplied.data(),count});if(!applied)return std::unexpected(applied.error());
        }else{auto applied=builder.with_attributes(mask);if(!applied)return std::unexpected(applied.error());}
    }
    return builder.freeze();
}
inline Result<void> validate_diagnostic_bits(std::uint32_t bits,bool cancel) noexcept {
    // §8.4.1.2.1: reserved 18..13 and bit0. User bits12..1 remain class-defined.
    if(bits&0x0007e001u)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(cancel && (bits&0x1ff80000u))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
template<BodyKind K> inline Result<std::size_t> append_fields(PacketView& packet,const IndicatorView& indicators,std::size_t values_offset,DiagnosticGroup group=DiagnosticGroup::none) noexcept {
    auto layout=layout_from<K>(indicators);if(!layout)return std::unexpected(layout.error());
    const auto payload=packet.envelope.payload;
    const auto own_header=indicators.bytes;
    auto measured=walk_layout(*layout,[&](LayoutElement e) noexcept -> Result<void> {
        auto offset=checked_add(values_offset,e.offset-own_header);if(!offset)return std::unexpected(offset.error());
        if(*offset>payload.size() || e.bytes>payload.size()-*offset)return std::unexpected(Error{ErrorCode::short_input,*offset,*offset+e.bytes});
        if constexpr(K==BodyKind::diagnostics){auto valid=validate_diagnostic_bits(detail::load32(payload,*offset),packet.envelope.envelope.cancel);if(!valid)return valid;}
        if constexpr(K==BodyKind::values){auto valid=detail::validate_wire_value(e.field,payload.subspan(*offset,e.bytes));if(!valid)return valid;}
        return packet.fields.push_back(FieldView{e.field,e.attribute,K,group,payload.subspan(*offset,e.bytes)});
    });
    if(!measured)return std::unexpected(measured.error());
    return measured->bytes-own_header;
}
inline Result<PacketView> decode_packet(Bytes wire,DecodeOptions options={}) noexcept {
    auto envelope=decode_envelope(wire);if(!envelope)return std::unexpected(envelope.error());
    if(options.expected_class && envelope->envelope.class_id!=options.expected_class)return std::unexpected(Error{ErrorCode::unsupported_capability});
    PacketView result{*envelope,{}};const auto& e=envelope->envelope;const auto payload=envelope->payload;
    if(is_data(e.type)||is_extension(e.type)){result.opaque=true;return result;}
    if(e.type==PacketType::context) {
        auto indicators=parse_indicators(payload,0,true);if(!indicators)return std::unexpected(indicators.error());result.change=indicators->change;
        auto size=append_fields<BodyKind::values>(result,*indicators,indicators->bytes);if(!size)return std::unexpected(size.error());
        if(indicators->bytes+*size!=payload.size())return std::unexpected(Error{ErrorCode::invalid_argument,indicators->bytes+*size});return result;
    }
    const auto cam=e.command->cam;
    if(!e.ack || (cam&(1u<<18))) {
        const bool selectors=!e.ack && (e.cancel||((cam>>23)&3)==0);
        // Permission 9.1.1-1 permits ordinary Control use (I4); cancellation and Acks remain separate.
        const bool allow_change=!e.ack && !e.cancel;
        auto indicators=parse_indicators(payload,0,allow_change);if(!indicators)return std::unexpected(indicators.error());
        result.change=indicators->change;
        std::size_t body=0;
        if(selectors){result.body_kind=BodyKind::selectors;auto snapshot=indicators->selectors.freeze();for(const auto& field:snapshot.fields()){
            const auto selected=snapshot.layout().attributes?snapshot.layout().attributes:attribute_bit(Attribute::current);
            for(unsigned a=0;a<13;++a)if(selected&attribute_bit(static_cast<Attribute>(a))){auto added=result.fields.push_back(FieldView{field.id,static_cast<Attribute>(a),BodyKind::selectors,DiagnosticGroup::none,{}});if(!added)return std::unexpected(added.error());}
        }}else{auto size=append_fields<BodyKind::values>(result,*indicators,indicators->bytes);if(!size)return std::unexpected(size.error());body=*size;}
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
    std::optional<IndicatorView> warnings,errors;std::size_t offset=0;
    if(warning_body){auto v=parse_indicators(payload,offset,false);if(!v)return std::unexpected(v.error());warnings=*v;offset+=v->bytes;}
    if(error_body){auto v=parse_indicators(payload,offset,false);if(!v)return std::unexpected(v.error());errors=*v;offset+=v->bytes;}
    if(warnings){auto n=append_fields<BodyKind::diagnostics>(result,*warnings,offset,DiagnosticGroup::warning);if(!n)return std::unexpected(n.error());offset+=*n;}
    if(errors){auto n=append_fields<BodyKind::diagnostics>(result,*errors,offset,DiagnosticGroup::error);if(!n)return std::unexpected(n.error());offset+=*n;}
    if(offset!=payload.size())return std::unexpected(Error{ErrorCode::invalid_argument,offset});return result;
}
// Delivers no application callbacks until the complete packet has validated.
template<class Visitor> inline Result<void> decode_and_visit(Bytes wire,DecodeOptions options,Visitor&& visitor) noexcept {
    auto parsed=decode_packet(wire,options);if(!parsed)return std::unexpected(parsed.error());
    for(std::size_t i=0;i<parsed->fields.size();++i){auto r=visitor(parsed->fields[i]);if(!r)return r;}return {};
}
namespace detail {
template<BodyKind K,std::size_t N> inline void write_indicators(const PacketSnapshot<K,N>& packet,MutableBytes output,bool change=false) noexcept {
    const auto& cif=packet.layout().cif;std::size_t offset=0;detail::store32(output,offset,cif[0]|(change?1u<<31:0));offset+=4;
    for(unsigned i=1;i<8;++i)if(cif[0]&(1u<<i)){detail::store32(output,offset,cif[i]);offset+=4;}
}
template<BodyKind K,std::size_t N> inline Result<void> write_values(const PacketSnapshot<K,N>& packet,MutableBytes output,std::size_t values_start,std::size_t own_header) noexcept {
    auto walked=walk_layout(packet,[&](LayoutElement e) noexcept -> Result<void>{
        const auto offset=values_start+e.offset-own_header;
        const FieldEntry* entry=nullptr;for(const auto& f:packet.fields())if(f.id==e.field){entry=&f;break;}
        if(!entry)return std::unexpected(Error{ErrorCode::invalid_state});
        if constexpr(K==BodyKind::diagnostics)detail::store32(output,offset,entry->diagnostic);
        else if constexpr(K==BodyKind::values){const auto& value=*entry->values[static_cast<unsigned>(e.attribute)];
            if(e.bytes==4)detail::store32(output,offset,std::get<std::uint32_t>(value));
            else{auto bits=std::holds_alternative<Hertz>(value)?std::bit_cast<std::uint64_t>(std::get<Hertz>(value).q20):std::get<PayloadFormat>(value).bits;detail::store32(output,offset,static_cast<std::uint32_t>(bits>>32));detail::store32(output,offset+4,static_cast<std::uint32_t>(bits));}
        }return {};
    });
    if(!walked)return std::unexpected(walked.error());return {};
}
} // namespace detail
template<BodyKind K,std::size_t N> inline Result<std::size_t> encode_packet(const Envelope& e,const PacketSnapshot<K,N>& snapshot,MutableBytes output,bool change=false) noexcept {
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
    auto body=measure(snapshot);if(!body)return std::unexpected(body.error());
    if constexpr(K==BodyKind::values) {
        std::array<std::byte,8> scratch{};
        for(const auto& field:snapshot.fields())for(const auto& value:field.values)if(value) {
            if(field.id==StateEvent::id)detail::store32(scratch,0,std::get<std::uint32_t>(*value));
            if(field.id==DataPayloadFormat::id)detail::store32(scratch,0,static_cast<std::uint32_t>(std::get<PayloadFormat>(*value).bits>>32));
            auto valid=detail::validate_wire_value(field.id,scratch);if(!valid)return std::unexpected(valid.error());
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
