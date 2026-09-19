#include <vita/codec/packet.hpp>
#include <vita/codec/samples.hpp>
#include <cassert>
#include <array>
#include <algorithm>
bool codec_other_tu() noexcept;
int main(){
    using namespace vita;using namespace vita::codec;
    assert(codec_other_tu());
    Envelope command{};command.type=PacketType::command;command.stream_id=1;command.command=Command{0xa0040000,1,Identifier::short_id(2),Identifier::short_id(3)};
    QueryPacket query;assert(query.select<SampleRate>());
    std::array<std::byte,512> storage{};
    auto encoded=encode_packet(command,query.freeze(),storage);assert(encoded&&*encoded==28);
    auto decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&decoded->fields.size()==1&&decoded->body_kind==BodyKind::selectors);
    ControlPacket control;assert(control.set<SampleRate>(*Hertz::from_integer(1'000'000)));command.command->cam=0xa90b0000;command.command->message_id=2;
    encoded=encode_packet(command,control.freeze(),storage);assert(encoded&&*encoded==36);
    decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&std::get<Hertz>(*decoded->fields[0].value()).q20==(1'000'000ll<<20));
    encoded=encode_packet(command,control.freeze(),storage,true);assert(encoded);
    decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&decoded->change&&decoded->fields.size()==1);
    int callbacks=0;auto visited=decode_and_visit(Bytes{storage}.first(*encoded-4),{},[&](const FieldView&) noexcept -> Result<void>{++callbacks;return {};});assert(!visited&&callbacks==0);
    std::array<std::byte,8> short_output{};short_output.fill(std::byte{0xaa});auto short_result=encode_packet(command,control.freeze(),short_output);assert(!short_result&&short_result.error().required_capacity==36);assert(short_output[0]==std::byte{0xaa});
    command.ack=true;command.command->cam=0xa9080400;
    DiagnosticAck empty;encoded=encode_diagnostic(command,empty.freeze(),empty.freeze(),RequestContext{0xa90b0000},storage);assert(encoded&&*encoded==24);assert(decode_packet(Bytes{storage}.first(*encoded)));
    DiagnosticAck warnings,errors;assert(warnings.diagnostic(SampleRate::id,1u<<27));assert(errors.diagnostic(ReferencePoint::id,1u<<29));command.command->cam=0xa90b0c00;
    encoded=encode_diagnostic(command,warnings.freeze(),errors.freeze(),RequestContext{0xa90b0000},storage);assert(encoded&&*encoded==40);
    decoded=decode_packet(Bytes{storage}.first(*encoded),DecodeOptions{RequestContext{0xa90b0000}});assert(decoded&&decoded->fields.size()==2&&decoded->fields[0].group==DiagnosticGroup::warning&&decoded->fields[1].group==DiagnosticGroup::error);
    auto unresolved=decode_packet(Bytes{storage}.first(*encoded));
    assert(unresolved && unresolved->opaque && unresolved->requires_request_context && unresolved->fields.empty());
    callbacks=0; visited=decode_and_visit(Bytes{storage}.first(*encoded),{},[&](const FieldView&) noexcept -> Result<void>{++callbacks;return {};});
    assert(visited && callbacks==0);
    std::array<Iq<std::int16_t>,2> iq{{{16384,0},{15137,6270}}};std::array<std::byte,8> payload{};assert(pack_iq16(iq,payload));
    auto iqview=SampleView<std::int16_t>::create(payload);assert(iqview&&*iqview->at(1)==iq[1]&&!iqview->at(2));
    for(unsigned type=0;type<8;++type){Envelope e{};e.type=static_cast<PacketType>(type);if(type!=0&&type!=2)e.stream_id=99;e.class_id=ClassId{0x123456,1,2,0};e.timestamp={Tsi::gps,Tsf::picoseconds,22,987654321};
        if(type>=6)e.command=Command{0xf9000000,12,Identifier::uuid({1,2,3,4}),Identifier::uuid({5,6,7,8})};
        if(type<=3)e.trailer=true;
        auto n=encode_envelope(e,payload,e.trailer?std::optional<std::uint32_t>{0x1234}:std::nullopt,storage);assert(n);auto view=decode_envelope(Bytes{storage}.first(*n));assert(view&&view->payload.size()==8&&view->envelope.timestamp.fractional==987654321);
    }
    // Overlap source starts where the header will be written.
    std::copy(payload.begin(),payload.end(),storage.begin());Envelope data{};data.stream_id=1;
    encoded=encode_envelope(data,Bytes{storage}.first(8),std::nullopt,storage);assert(encoded&&*encoded==16);assert(std::equal(payload.begin(),payload.end(),storage.begin()+8));
    assert(query.with_attributes(0x8c000000));command.ack=false;command.command->cam=0xa0040000;
    encoded=encode_packet(command,query.freeze(),storage);assert(encoded);
    decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&decoded->fields.size()==3&&decoded->fields[1].attribute==Attribute::maximum);
    command.command->cam=0xa90b0000;encoded=encode_packet(command,control.freeze(),storage);assert(encoded);
    for(std::size_t i=*encoded-8;i<*encoded;++i)storage[i]=std::byte{0xff};
    decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&std::get<Hertz>(*decoded->fields[0].value()).q20==-1);
    ContextPacket context;assert(context.set<ReferencePoint>(123));assert(context.set<SampleRate>(*Hertz::from_integer(1)));assert(context.set<StateEvent>(0xc00c0000));assert(context.set<DataPayloadFormat>(PayloadFormat{0x200003cf00000000ull}));Envelope ce{};ce.type=PacketType::context;ce.stream_id=123;
    encoded=encode_packet(ce,context.freeze(),storage,true);assert(encoded);decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&decoded->change&&decoded->fields.size()==4);
    std::array<Iq<std::int32_t>,1> iq32{{{INT32_MIN,INT32_MAX}}};assert(pack_iq32(iq32,payload));assert(*SampleView<std::int32_t>::create(payload)->at(0)==iq32[0]);
    std::array<Iq<float>,1> iqfloat{{{0.5f,-0.0f}}};assert(pack_iq_float(iqfloat,payload));assert(*SampleView<float>::create(payload)->at(0)==iqfloat[0]);
    return 0;
}
