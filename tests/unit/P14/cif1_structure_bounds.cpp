#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
using namespace vita;using namespace vita::codec;
static_assert(!std::is_constructible_v<NativeRecordView<SectorRecord>,Bytes>);
static_assert(!std::is_constructible_v<NativeRecordView<PointingVectorRecord>,Bytes>);
static_assert(std::is_default_constructible_v<NativeRecordView<SectorRecord>>);
static_assert(std::is_copy_constructible_v<NativeRecordView<SectorRecord>>);
int main() {
    std::array<std::byte,4096> bytes{};
    // Literal257-record Pointing packet: header code3, one word per record.
    const std::array<std::uint32_t,7> header{0x40000108,1,2,0x10000000,260,0x03001101,0x40000000};
    for(unsigned i=0;i<header.size();++i)for(unsigned b=0;b<4;++b)bytes[i*4+b]=std::byte((header[i]>>(24-8*b))&255);
    auto wire=Bytes{bytes}.first(1056);auto rejected=decode_packet(wire);assert(!rejected&&rejected.error().code==ErrorCode::resource_limit);
    DecodeOptions limits;limits.limits.records=257;auto parsed=decode_packet(wire,limits);assert(parsed&&*parsed->fields[0].pointing_vectors()->size()==257);
    NativeContextPacket<65536> materialized;assert(!parsed->fields[0].materialize_into(materialized));
    // Wrong declared record width is malformed even when record count exceeds default.
    bytes[21]=std::byte{0};bytes[22]=std::byte{0x21};rejected=decode_packet(wire);assert(!rejected&&rejected.error().code==ErrorCode::invalid_argument);
    // Independent native attributes have separate header and record shapes.
    PointingVectorRecord one{{1,2},{}};std::array<PointingVectorRecord,2> two{{one,one}};
    NativeContextPacket<128> p;assert(p.set<PointingVectorStructure>({{},false,{&one,1}}));auto old=p.freeze();
    const AttributeInput average{PointingVectorStructure::id,Attribute::average,PointingVectorInput{PointingReference{0,2,1},false,two}};
    const auto mask=attribute_bit(Attribute::current)|attribute_bit(Attribute::average);assert(p.with_attribute_inputs(mask,{&average,1}));auto snapshot=p.freeze();
    assert(snapshot.get<PointingVectorStructure>()->records.size()==1);assert(snapshot.get<PointingVectorStructure>(Attribute::average)->records.size()==2);assert(old.get<PointingVectorStructure>()->records.size()==1);
    Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;auto n=encode_packet(envelope,snapshot,bytes);assert(n);parsed=decode_packet(Bytes{bytes}.first(*n));assert(parsed&&parsed->fields.size()==2);assert(parsed->fields[0].bytes.size()==16&&parsed->fields[1].bytes.size()==24);
    assert(parsed->fields[1].materialize_into(p));
    const auto before=p.freeze();const AttributeInput missing{PointingVectorStructure::id,Attribute::average,PointingVectorInput{{},true,two}};assert(!p.with_attribute_inputs(mask,{&missing,1}));assert(p.generation()==before.generation());
    auto cached=measure(snapshot);assert(cached);assert(p.set_attribute<PointingVectorStructure>(Attribute::average,{{},false,two}));assert(!validate_measure(p.freeze(),*cached));
    // Probability-only new structured IDs are one word and require no base values.
    ContextPacket probability;assert(probability.with_attributes(attribute_bit(Attribute::probability)));
    for(auto id:{PointingVectorStructure::id,IndexList::id,Spectrum::id,SectorStepScan::id}) {
        const AttributeInput value{id,Attribute::probability,SemanticValue{ProbabilityCode{3,2}}};
        if(id==PointingVectorStructure::id)assert(probability.set_field_attributes<PointingVectorStructure>({&value,1}));
        if(id==IndexList::id)assert(probability.set_field_attributes<IndexList>({&value,1}));
        if(id==Spectrum::id)assert(probability.set_field_attributes<Spectrum>({&value,1}));
        if(id==SectorStepScan::id)assert(probability.set_field_attributes<SectorStepScan>({&value,1}));
    }
    n=encode_packet(envelope,probability.freeze(),bytes);assert(n);parsed=decode_packet(Bytes{bytes}.first(*n));assert(parsed&&parsed->fields.size()==4);
    for(unsigned i=0;i<4;++i)assert(parsed->fields[i].bytes.size()==4&&parsed->fields[i].value());
    // Index1024 is supported,1025 is a configured native capacity limit.
    std::array<std::uint32_t,1025> indices{};NativeContextPacket<8192> list;assert(list.set<IndexList>({1,{indices.data(),1024}}));assert(!list.replace<IndexList>({1,indices}));
    auto list_snapshot=list.freeze();assert(list_snapshot.get<IndexList>()->size()==1024);
    n=encode_packet(envelope,list_snapshot,bytes);assert(n);limits={};limits.limits.index_entries=1023;rejected=decode_packet(Bytes{bytes}.first(*n),limits);assert(!rejected&&rejected.error().code==ErrorCode::resource_limit);
    std::array<std::byte,4096> out{};out.fill(std::byte{0xa5});auto untouched=out;assert(!encode_packet(envelope,list_snapshot,MutableBytes{out}.first(*n-1)));assert(out==untouched);
}
