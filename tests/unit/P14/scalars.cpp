#include <vita/codec/packet.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
using namespace vita;
using namespace vita::codec;

struct Vector { FieldId id; SemanticValue value; std::uint64_t wire; };
const std::array vectors{
    Vector{Bandwidth::id,Hertz{1},0x0000000000000001ull},
    Vector{IFReferenceFrequency::id,Hertz{-1048576},0xfffffffffff00000ull},
    Vector{RFReferenceFrequency::id,Hertz{1048576},0x0000000000100000ull},
    Vector{RFReferenceFrequencyOffset::id,Hertz{-1},0xffffffffffffffffull},
    Vector{IFBandOffset::id,Hertz{INT64_MIN},0x8000000000000000ull},
    Vector{ReferenceLevel::id,DecibelsQ7{-128},0x0000ff80ull},
    Vector{Gain::id,GainStages{-128,256},0x0100ff80ull},
    Vector{OverRangeCount::id,std::uint32_t{UINT32_MAX},0xffffffffull},
    Vector{TimestampAdjustment::id,Femtoseconds{-1},0xffffffffffffffffull},
    Vector{TimestampCalibrationTime::id,std::uint32_t{0x12345678},0x12345678ull},
    Vector{Temperature::id,CelsiusQ6{-64},0x0000ffc0ull},
    Vector{DeviceIdentifier::id,DeviceIdentifierValue{0x123456,0xabcd},0x001234560000abcdull},
    Vector{EphemerisReferenceId::id,std::uint32_t{0x87654321},0x87654321ull}};

int main() {
    static_assert(sizeof(SemanticValue)==16);
    static_assert(baseline_descriptors.size()==4);
    static_assert(scalar_descriptors.size()==13);
    Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;
    std::array<std::byte,256> buffer{};
    for(const auto& v:vectors) {
        ContextPacket packet;assert(packet.set_value(v.id,v.value));
        auto encoded=encode_packet(envelope,packet.freeze(),buffer);assert(encoded);
        const auto bytes=descriptor(v.id)->words*4;
        assert(*encoded==12+bytes);
        for(std::size_t i=0;i<bytes;++i)
            assert(buffer[12+i]==std::byte((v.wire>>(8*(bytes-1-i)))&255));
        auto decoded=decode_packet(Bytes{buffer}.first(*encoded));assert(decoded);
        assert(decoded->fields.size()==1 && *decoded->fields[0].value()==v.value);
        for(std::size_t length=0;length<*encoded;++length) {
            assert(!decode_packet(Bytes{buffer}.first(length)));
            std::array<std::byte,256> output;output.fill(std::byte{0xa5});
            auto short_result=encode_packet(envelope,packet.freeze(),MutableBytes{output}.first(length));
            assert(!short_result && short_result.error().code==ErrorCode::short_output);
            assert(std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0xa5};}));
        }
        assert(packet.with_attributes(attribute_bit(Attribute::current)));
        assert(encode_packet(envelope,packet.freeze(),buffer));
        const auto generation=packet.generation();
        assert(!packet.with_attributes(attribute_bit(Attribute::average)));
        assert(packet.generation()==generation);
    }
    ContextPacket combined;
    for(auto it=vectors.rbegin();it!=vectors.rend();++it)assert(combined.set_value(it->id,it->value));
    auto encoded=encode_packet(envelope,combined.freeze(),buffer);assert(encoded&&*encoded==92);
    auto parsed=decode_packet(Bytes{buffer}.first(*encoded));assert(parsed&&parsed->fields.size()==13);
    for(std::size_t i=0;i<vectors.size();++i)assert(parsed->fields[i].id==vectors[i].id);

    ContextPacket checked;
    assert(!checked.set<Bandwidth>(Hertz{-1}));
    assert(!checked.set<Temperature>(CelsiusQ6{-17482}));
    assert(checked.set<Temperature>(CelsiusQ6{-17481}));
    assert(!checked.set<DeviceIdentifier>(DeviceIdentifierValue{0x1000000,0}));
    assert(!checked.set_value(Gain::id,std::uint32_t{}));
    assert(!checked.set_value({0,14},std::uint32_t{}));
    for(const auto id:{ReferenceLevel::id,Temperature::id,DeviceIdentifier::id}) {
        ContextPacket p;assert(p.set_value(id,placeholder(id)));
        auto n=encode_packet(envelope,p.freeze(),buffer);assert(n);
        buffer[12]=std::byte{0x80};assert(!decode_packet(Bytes{buffer}.first(*n)));
        if(id==DeviceIdentifier::id){buffer[12]=std::byte{};buffer[16]=std::byte{1};assert(!decode_packet(Bytes{buffer}.first(*n)));}
    }
    for(const auto id:{Bandwidth::id,Temperature::id}) {
        ContextPacket p;assert(p.set_value(id,placeholder(id)));
        auto n=encode_packet(envelope,p.freeze(),buffer);assert(n);
        if(id==Bandwidth::id)std::fill(buffer.begin()+12,buffer.begin()+20,std::byte{0xff});
        else {buffer[14]=std::byte{0x80};buffer[15]=std::byte{};}
        auto raw=decode_packet(Bytes{buffer}.first(*n));assert(raw);
        auto value=raw->fields[0].value();assert(value&&!validate_value(id,*value));
    }
    // Every fixed-width endpoint survives encode/decode without native rounding.
    for (auto value : {SemanticValue{GainStages{INT16_MIN,INT16_MAX}},
                       SemanticValue{Femtoseconds{INT64_MIN}}, SemanticValue{Femtoseconds{INT64_MAX}},
                       SemanticValue{DecibelsQ7{INT16_MIN}}, SemanticValue{DecibelsQ7{INT16_MAX}},
                       SemanticValue{CelsiusQ6{INT16_MAX}},
                       SemanticValue{DeviceIdentifierValue{0xffffff,0xffff}}}) {
        const FieldId id = value.index()==4 ? Gain::id : value.index()==5 ? TimestampAdjustment::id :
                           value.index()==3 ? ReferenceLevel::id : value.index()==6 ? Temperature::id : DeviceIdentifier::id;
        ContextPacket p;assert(p.set_value(id,value));
        auto n=encode_packet(envelope,p.freeze(),buffer);assert(n);
        auto view=decode_packet(Bytes{buffer}.first(*n));assert(view&&*view->fields[0].value()==value);
    }
    // Control and State-Ack semantics consume the same new typed scalars.
    Envelope active;active.type=PacketType::command;active.stream_id=1;
    active.command=Command{0xa90b0000,9,Identifier::short_id(2),Identifier::short_id(3)};
    ControlPacket setter;assert(setter.set<Gain>(GainStages{-1,1}));
    auto setsize=encode_packet(active,setter.freeze(),buffer);assert(setsize);
    assert(decode_packet(Bytes{buffer}.first(*setsize)));
    StateAck state;assert(state.configure(0,2));assert(state.set<Gain>(GainStages{-1,1}));
    active.ack=true;active.command->cam=0xa9040400;
    auto statesize=encode_packet(active,state.freeze(),buffer);assert(statesize);
    assert(decode_packet(Bytes{buffer}.first(*statesize)));
    // New selectors and diagnostic layouts share the same registry.
    QueryPacket query;assert(query.select<DeviceIdentifier>());assert(query.select<Gain>());
    Envelope command;command.type=PacketType::command;command.stream_id=1;
    command.command=Command{0xa0040000,1,Identifier::short_id(2),Identifier::short_id(3)};
    encoded=encode_packet(command,query.freeze(),buffer);assert(encoded&&*encoded==28);
    parsed=decode_packet(Bytes{buffer}.first(*encoded));assert(parsed&&parsed->fields.size()==2);
    DiagnosticAck warnings,errors;assert(warnings.diagnostic(Temperature::id,1u<<27));
    assert(errors.diagnostic(DeviceIdentifier::id,1u<<29));
    command.ack=true;command.command->cam=0xa90b0c00;
    encoded=encode_diagnostic(command,warnings.freeze(),errors.freeze(),RequestContext{0xa90b0000},buffer);assert(encoded);
    parsed=decode_packet(Bytes{buffer}.first(*encoded),DecodeOptions{RequestContext{0xa90b0000}});
    assert(parsed&&parsed->fields.size()==2&&*parsed->fields[1].diagnostic()==(1u<<29));
}
