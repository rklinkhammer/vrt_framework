#include <vita/codec/packet.hpp>
#include "vectors.hpp"
#include <algorithm>
#include <vector>
using namespace vita;using namespace vita::codec;
static Envelope command(std::uint32_t cam,std::uint32_t mid,bool ack=false,bool cancel=false){
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.ack=ack;e.cancel=cancel;
    e.command=Command{cam,mid,Identifier::short_id(2),Identifier::short_id(3)};return e;
}
int main(){
    auto query=decode_packet(w1),control=decode_packet(w2),cancel=decode_packet(w3),ack=decode_packet(w4),data=decode_packet(w5);
    if(!query||!control||!cancel||!ack||!data)return 1;
    if(query->body_kind!=BodyKind::selectors || query->fields.size()!=1 || !query->fields[0].bytes.empty() || query->fields[0].value())return 2;
    if(control->fields.size()!=1 || std::get<Hertz>(*control->fields[0].value()).q20!=1048576000000LL)return 3;
    if(cancel->body_kind!=BodyKind::selectors || ack->fields.size()!=0 || !data->opaque || data->envelope.payload.size()!=8)return 4;
    std::array<std::byte,128> out{};
    QueryPacket q;q.select<SampleRate>();auto encoded=encode_packet(command(0xa0040000,1),q.freeze(),out);
    if(!encoded || *encoded!=w1.size() || !std::equal(w1.begin(),w1.end(),out.begin()))return 5;
    ControlPacket c;c.set<SampleRate>(*Hertz::from_integer(1000000));encoded=encode_packet(command(0xa90b0000,2),c.freeze(),out);
    if(!encoded || *encoded!=w2.size() || !std::equal(w2.begin(),w2.end(),out.begin()))return 6;
    CancelPacket k;k.select<SampleRate>();encoded=encode_packet(command(0xa9080000,2,false,true),k.freeze(),out);
    if(!encoded || *encoded!=w3.size() || !std::equal(w3.begin(),w3.end(),out.begin()))return 7;
    DiagnosticAck empty;encoded=encode_diagnostic(command(0xa9080400,2,true),empty.freeze(),empty.freeze(),RequestContext{0xa90b0000},out);
    if(!encoded || *encoded!=w4.size() || !std::equal(w4.begin(),w4.end(),out.begin()))return 8;
    unsigned callbacks=0;auto visit=[&](FieldView) noexcept->Result<void>{++callbacks;return {};};
    if(decode_and_visit(Bytes{w2}.first(w2.size()-4),{},visit) || callbacks)return 9;
    const auto surplus=wire_bytes(std::array<std::uint32_t,9>{0x60000009,1,0xa0040000,1,2,3,0x00200000,0,0});
    if(decode_and_visit(surplus,{},visit) || callbacks)return 10;
    const auto reserved=wire_bytes(std::array<std::uint32_t,1>{0x80000001});
    if(decode_and_visit(reserved,{},visit) || callbacks)return 11;
    const auto diagnostic=wire_bytes(std::array<std::uint32_t,10>{0x6400000a,1,0xa90b0400,2,2,3,0x00200000,0x40000000,0x80000000,0x40000000});
    auto unresolved=decode_packet(diagnostic);
    if(!unresolved || !unresolved->opaque || !unresolved->requires_request_context || !unresolved->fields.empty() || unresolved->body_kind!=BodyKind::diagnostics || unresolved->envelope.payload.size()!=16)return 12;
    callbacks=0;if(!decode_and_visit(diagnostic,{},visit) || callbacks)return 23;
    auto details=decode_packet(diagnostic,DecodeOptions{RequestContext{0xa90b0000}});
    if(!details || details->fields.size()!=2 || details->fields[0].group!=DiagnosticGroup::warning || details->fields[1].group!=DiagnosticGroup::error ||
       details->fields[0].bytes.size()!=4 || *details->fields[0].diagnostic()!=0x80000000 || *details->fields[1].diagnostic()!=0x40000000)return 13;
    DiagnosticAck warnings,errors;warnings.diagnostic(SampleRate::id,0x80000000);errors.diagnostic(ReferencePoint::id,0x40000000);
    encoded=encode_diagnostic(command(0xa90b0400,2,true),warnings.freeze(),errors.freeze(),RequestContext{0xa90b0000},out);
    if(!encoded || *encoded!=diagnostic.size() || !std::equal(diagnostic.begin(),diagnostic.end(),out.begin()))return 14;
    const auto summary=wire_bytes(std::array<std::uint32_t,6>{0x64000006,1,0xa9090400,2,2,3});
    auto summary_only=decode_packet(summary,DecodeOptions{RequestContext{0xa9080000}});
    if(!summary_only || summary_only->fields.size()!=0)return 15;
    const auto attributes=wire_bytes(std::array<std::uint32_t,8>{0x60000008,1,0xa0040000,1,2,3,0x00200080,0x8c000000});
    auto selected=decode_packet(attributes);
    if(!selected || selected->fields.size()!=3 || selected->fields[0].attribute!=Attribute::current || selected->fields[1].attribute!=Attribute::maximum || selected->fields[2].attribute!=Attribute::minimum)return 16;
    auto negative=w2;for(unsigned i=28;i<32;++i)negative[i]=std::byte{0xff};negative[32]=std::byte{0xff};negative[33]=std::byte{0xf0};negative[34]=negative[35]=std::byte{0};
    auto raw_negative=decode_packet(negative);
    if(!raw_negative || std::get<Hertz>(*raw_negative->fields[0].value()).q20!=-1048576LL || validate_value(SampleRate::id,*raw_negative->fields[0].value()))return 17;
    auto wrongclass=decode_packet(w1,DecodeOptions{std::nullopt,ClassId{1,1,1,0}});if(wrongclass)return 18;
    const auto unknown=wire_bytes(std::array<std::uint32_t,5>{0x40000005,1,0x60000000,9,10});
    callbacks=0;if(decode_and_visit(unknown,{},visit) || callbacks)return 19;
    const auto state_reserved=wire_bytes(std::array<std::uint32_t,4>{0x40000004,1,0x00010000,0x00100000});
    if(decode_packet(state_reserved))return 20;
    const auto dpf_reserved=wire_bytes(std::array<std::uint32_t,5>{0x40000005,1,0x00008000,0x580003cf,0});
    if(decode_packet(dpf_reserved))return 21;
    auto reserved_cam=w1;reserved_cam[9]|=std::byte{0x20};if(decode_packet(reserved_cam))return 22;
    // Literal CIF0 high bit is permitted for ordinary Control, independently of encoder output.
    for(unsigned action=0;action<3;++action){
        auto changed=action==0?std::vector<std::byte>(w1.begin(),w1.end()):std::vector<std::byte>(w2.begin(),w2.end());
        changed[8]=std::byte((std::to_integer<unsigned>(changed[8])&~1u)|(action>>1));
        changed[9]=std::byte((std::to_integer<unsigned>(changed[9])&~0x80u)|((action&1u)<<7));
        changed[24]|=std::byte{0x80};auto parsed=decode_packet(changed);
        if(!parsed || !parsed->change)return 24;
    }
    auto bad_cancel=w3;bad_cancel[24]|=std::byte{0x80};if(decode_packet(bad_cancel))return 25;
    const auto changed_state=wire_bytes(std::array<std::uint32_t,9>{0x64000009,1,0xa9040000,2,2,3,0x80200000,0x000000f4,0x24000000});
    if(decode_packet(changed_state))return 26;
    auto bad_diagnostic=diagnostic;bad_diagnostic[24]|=std::byte{0x80};
    if(decode_packet(bad_diagnostic,DecodeOptions{RequestContext{0xa90b0000}}))return 27;
    encoded=encode_packet(command(0xa90b0000,2),c.freeze(),out,true);
    if(!encoded || out[24]!=std::byte{0x80})return 28;
    if(encode_packet(command(0xa9080000,2,false,true),k.freeze(),out,true))return 29;
    return 0;
}
