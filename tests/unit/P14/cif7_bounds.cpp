#include <vita/codec/packet.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <array>
#include <cassert>
#include <cstdio>
using namespace vita;
using namespace vita::codec;
int main() {
    TypedPacket<BodyKind::values,PacketSubtype::context,128> values;
    constexpr std::array<unsigned,20> bits{30,29,28,27,26,25,23,21,20,18,17,16,15,13,12,11,10,9,8,7};
    std::array<AttributeValue,260> supplied{};unsigned index=0;
    for(auto bit:bits){const FieldId id{2,static_cast<std::uint8_t>(bit)};assert(values.set_value(id,std::uint32_t{1}));
        for(unsigned a=0;a<13;++a)supplied[index++]={id,static_cast<Attribute>(a),a==11?SemanticValue{ProbabilityCode{1,0}}:a==12?SemanticValue{BeliefCode{2}}:SemanticValue{std::uint32_t{1}}};}
    EditWorkspace<BodyKind::values,128,0> workspace;assert(values.with_attributes(all_attributes,supplied,workspace));
    std::array<std::byte,8192> wire{};Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;
    auto n=encode_packet(envelope,values.freeze(),wire);assert(n);
    auto parsed=decode_packet_bounded<128,1664>(Bytes{wire}.first(*n));assert(parsed&&parsed->fields.size()==260);
    assert((!decode_packet_bounded<128,259>(Bytes{wire}.first(*n))));
    auto limits=DecodeOptions{};limits.limits.work_units=259;assert((!decode_packet_bounded<128,1664>(Bytes{wire}.first(*n),limits)));
    limits.limits.work_units=260;assert((decode_packet_bounded<128,1664>(Bytes{wire}.first(*n),limits)));
    TypedPacket<BodyKind::diagnostics,PacketSubtype::diagnostic_ack,128> warning,error;
    unsigned count=0;
    for(unsigned cif=0;cif<4 && count<65;++cif)for(int bit=31;bit>=0 && count<65;--bit) {
        const FieldId id{static_cast<std::uint8_t>(cif),static_cast<std::uint8_t>(bit)};if(!descriptor(id))continue;
        assert(warning.diagnostic(id,0));if(count<64)assert(error.diagnostic(id,0));++count;
    }
    assert(count==65);envelope.type=PacketType::command;envelope.ack=true;envelope.command=Command{0xa0000000u|(1u<<20)|(1u<<17)|(1u<<16),1,Identifier::short_id(2),Identifier::short_id(3)};
    const RequestContext request{(1u<<17)|(1u<<16)};
    n=encode_diagnostic(envelope,warning.freeze(),error.freeze(),request,wire);assert(n);
    DecodeOptions options;options.request=request;
    assert((!decode_packet_bounded<128,1664>(Bytes{wire}.first(*n),options)));
    assert((decode_packet_bounded<129,1664>(Bytes{wire}.first(*n),options)));
    const auto first=warning.freeze().fields()[0].id;assert(warning.remove(first));
    n=encode_diagnostic(envelope,warning.freeze(),error.freeze(),request,wire);assert(n);
    parsed=decode_packet_bounded<128,1664>(Bytes{wire}.first(*n),options);assert(parsed&&parsed->fields.size()==128);
    // Non-Current GPS latitude uses raw signed representation without Current range.
    NativeContextPacket<256> gps;GeolocationValue location{};location.latitude_q22=91*(1<<22);
    assert(!gps.set<FormattedGPS>(location));assert(gps.with_attributes(attribute_bit(Attribute::first_derivative)));
    const AttributeInput derivative{FormattedGPS::id,Attribute::first_derivative,location};assert(gps.set_field_attributes<FormattedGPS>({&derivative,1}));
    envelope={};envelope.type=PacketType::context;envelope.stream_id=1;n=encode_packet(envelope,gps.freeze(),wire);assert(n);
    auto view=decode_packet(Bytes{wire}.first(*n));assert(view);assert(view->fields[0].materialize_into(gps));
    auto snapshot=gps.freeze();assert(snapshot.get<FormattedGPS>(Attribute::first_derivative)->latitude_q22==location.latitude_q22);
    std::printf("Semantic=%zu Entry=%zu ScalarSnapshot=%zu NativeSnapshot=%zu DefaultView=%zu GenericView=%zu GenericSnapshot=%zu GenericWorkspace=%zu Input=%zu Plan=%zu\n",
      sizeof(SemanticValue),sizeof(FieldEntry),sizeof(PacketSnapshot<BodyKind::values>),sizeof(PacketSnapshot<BodyKind::values,16,8192>),sizeof(PacketView),sizeof(BasicPacketView<1664>),sizeof(PacketSnapshot<BodyKind::values,128,8192>),sizeof(EditWorkspace<BodyKind::values,128,8192>),sizeof(AttributeInput),sizeof(IndicatorPlan<128>));
}
