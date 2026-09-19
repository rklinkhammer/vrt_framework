#include <vita/codec/layout.hpp>
#include <vita/fields/arena.hpp>
#include <array>
#include <limits>
#include <type_traits>
using namespace vita;
static_assert(!std::is_convertible_v<std::uint32_t,Hertz>);
static_assert(!std::is_same_v<ContextPacket,ControlPacket>);
static_assert(!std::is_same_v<QueryPacket,CancelPacket>);
int main() {
    if(ControlPacket{}.freeze().layout().subtype!=PacketSubtype::control ||
       StateAck{}.freeze().layout().subtype!=PacketSubtype::state_ack ||
       SignalMetadata{}.freeze().layout().subtype!=PacketSubtype::signal ||
       CancelPacket{}.freeze().layout().subtype!=PacketSubtype::cancel)return 1;
    ContextPacket packet;
    const Hertz rate=*Hertz::from_integer(1000000);
    if(!packet.set<SampleRate>(rate))return 2;
    const auto old=packet.freeze();const auto cached=measure(old);
    if(!cached || cached->bytes!=12 || old.fields().size()!=1 || old.layout().cif[0]!=(1U<<21))return 3;
    const auto attributes=attribute_bit(Attribute::current)|attribute_bit(Attribute::minimum)|attribute_bit(Attribute::maximum);
    auto missing=packet.with_attributes(attributes);
    if(missing || packet.generation()!=old.generation() || !validate_measure(packet.freeze(),*cached))return 4;
    std::array supplied{AttributeValue{SampleRate::id,Attribute::minimum,*Hertz::from_integer(1)},
                        AttributeValue{SampleRate::id,Attribute::maximum,*Hertz::from_integer(100000000)}};
    if(!packet.with_attributes(attributes,supplied))return 5;
    const auto changed=packet.freeze();auto expanded=measure(changed);
    if(!expanded || expanded->bytes!=32 || validate_measure(changed,*cached))return 6;
    if(old.layout().attributes!=0 || std::get<Hertz>(*old.fields()[0].values[0])!=rate || !validate_measure(old,*cached))return 7;
    const auto old_generation=packet.generation();
    supplied[1]=supplied[0];
    if(packet.with_attributes(attributes,supplied) || packet.generation()!=old_generation)return 8;
    if(!packet.with_attributes(0) || !packet.replace<SampleRate>(*Hertz::from_integer(2)))return 9;
    if(!packet.set<ReferencePoint>(9) || !packet.set<StateEvent>(0x1234))return 10;
    auto ordered=packet.freeze();
    if(ordered.fields()[0].id!=ReferencePoint::id || ordered.fields()[1].id!=SampleRate::id || ordered.fields()[2].id!=StateEvent::id)return 11;
    auto index=index_layout<3>(ordered);
    if(!index || index->size()!=3 || (*index)[0].offset!=4 || (*index)[1].offset!=8 || (*index)[2].offset!=16)return 12;
    if(index_layout<2>(ordered))return 13;
    QueryPacket query;
    if(!query.select<SampleRate>() || !query.with_attributes(attributes))return 14;
    auto query_measure=measure(query.freeze());
    if(!query_measure || query_measure->bytes!=8 || !index_layout<0>(query.freeze()))return 15;
    DiagnosticAck diagnostic;
    if(!diagnostic.diagnostic(SampleRate::id,0x1234))return 16;
    auto diagnostic_measure=measure(diagnostic.freeze());
    if(!diagnostic_measure || diagnostic_measure->bytes!=8)return 17;
    PacketBuilder<BodyKind::values,1> bounded;
    if(!bounded.set<SampleRate>(rate))return 18;
    auto before=bounded.freeze();
    if(bounded.set<ReferencePoint>(5) || bounded.generation()!=before.generation() || bounded.freeze().fields().size()!=1)return 19;
    QueryPacket unknown;
    if(!unknown.select(FieldId{1,30}))return 20;
    auto unsupported=measure(unknown.freeze());
    if(unsupported || unsupported.error().code!=ErrorCode::unsupported_layout)return 21;
    if(query.select(FieldId{9,31}) || query.select(FieldId{0,32}))return 22;
    const auto maximum=std::numeric_limits<std::size_t>::max();
    if(measure(old,maximum-5) || checked_add(maximum,1) || checked_multiply(maximum,2))return 23;
    if(!checked_add(maximum,0) || !checked_multiply(maximum,0))return 24;
    SemanticArena<3> arena;std::array<std::uint32_t,2> words{4,5};
    auto slice=arena.append(words);if(!slice || arena.append(words) || arena.size()!=2)return 25;
    auto view=arena.view(*slice);if(!view || (*view)[0]!=4 || (*view)[1]!=5 || arena.view({maximum,2}))return 26;
    static_assert(!std::is_constructible_v<PacketBuilder<BodyKind::values>,PacketSubtype>);
    if(query.select(FieldId{0,7}) || query.select(FieldId{0,1}))return 27;
    return 0;
}
