#include <vita/codec/packet.hpp>
#include <algorithm>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
struct Vector { FieldId id;SemanticValue value;std::uint32_t wire; };
const std::array vectors{
    Vector{Bind::id,std::uint32_t{1},1},
    Vector{CitedSID::id,std::uint32_t{0x11223344},0x11223344},
    Vector{SiblingSID::id,std::uint32_t{0xdeadbeef},0xdeadbeef},
    Vector{ParentSID::id,std::uint32_t{0x80000000},0x80000000},
    Vector{ChildSID::id,std::uint32_t{0xffffffff},0xffffffff},
    Vector{CitedMessageId::id,std::uint32_t{0x12345678},0x12345678},
    Vector{ControlleeId::id,std::uint32_t{0x10203040},0x10203040},
    Vector{ControllerId::id,std::uint32_t{0x50607080},0x50607080},
    Vector{InformationSource::id,std::uint32_t{0x87654321},0x87654321},
    Vector{TrackId::id,std::uint32_t{0xabcdef01},0xabcdef01},
    Vector{CountryCode::id,CountryCodeValue{840,true},0x00008348},
    Vector{OperatorId::id,std::uint32_t{0xffff},0x0000ffff},
    Vector{PlatformClass::id,std::uint32_t{0x89abcdef},0x89abcdef},
    Vector{PlatformInstance::id,std::uint32_t{0xaabbccdd},0xaabbccdd},
    Vector{PlatformDisplay::id,std::uint32_t{0xeeff0011},0xeeff0011},
    Vector{EmsDeviceClass::id,EmsDeviceClassValue{0xabc,2,true,false},0x0000aabc},
    Vector{EmsDeviceType::id,std::uint32_t{0x01234567},0x01234567},
    Vector{EmsDeviceInstance::id,std::uint32_t{0xfedcba98},0xfedcba98},
    Vector{ModulationClass::id,std::uint32_t{0xffff},0x0000ffff},
    Vector{ModulationType::id,std::uint32_t{0x1234},0x00001234},
    Vector{FunctionId::id,std::uint32_t{0x2345},0x00002345},
    Vector{ModeId::id,std::uint32_t{0x3456},0x00003456},
    Vector{EventId::id,std::uint32_t{0x4567},0x00004567},
    Vector{FunctionPriority::id,std::uint32_t{0x5678},0x00005678},
    Vector{CommunicationPriority::id,std::uint32_t{0x89abcdef},0x89abcdef},
    Vector{RFFootprint::id,std::uint32_t{0xdeadbeef},0xdeadbeef},
    Vector{RFFootprintRange::id,std::uint32_t{0xcafebabe},0xcafebabe}};
void check_word(Bytes bytes,std::size_t offset,std::uint32_t expected) {
    for(unsigned i=0;i<4;++i)assert(bytes[offset+i]==std::byte((expected>>(24-8*i))&255));
}
int main() {
    static_assert(sizeof(SemanticValue)==16);
    static_assert(cif2_descriptors.size()==29);
    static_assert(std::is_same_v<std::variant_alternative_t<21,SemanticValue>,BufferSizeValue>);
    std::array<std::byte,512> buffer{};Envelope e;e.type=PacketType::context;e.stream_id=1;
    for(const auto& vector:vectors) {
        ContextPacket p;assert(p.set_value(vector.id,vector.value));
        auto n=encode_packet(e,p.freeze(),buffer);assert(n&&*n==20);
        check_word(buffer,8,4);check_word(buffer,12,1u<<vector.id.bit);check_word(buffer,16,vector.wire);
        auto decoded=decode_packet(Bytes{buffer}.first(*n));assert(decoded&&decoded->fields.size()==1);
        assert(*decoded->fields[0].value()==vector.value);
        ContextPacket copy;assert(decoded->fields[0].materialize_into(copy));
        for(std::size_t size=0;size<*n;++size) {
            assert(!decode_packet(Bytes{buffer}.first(size)));
            std::array<std::byte,512> short_output;short_output.fill(std::byte{0xab});
            assert(!encode_packet(e,p.freeze(),MutableBytes{short_output}.first(size)));
            assert(std::all_of(short_output.begin(),short_output.end(),[](auto b){return b==std::byte{0xab};}));
        }
        assert(p.with_attributes(attribute_bit(Attribute::current)));
        assert(!p.with_attributes(attribute_bit(Attribute::minimum)));
    }
    for(auto code:{CountryCodeValue{0,false},CountryCodeValue{0x7ff,false},CountryCodeValue{0x7ff,true}}) {
        ContextPacket p;assert(p.set<CountryCode>(code));auto n=encode_packet(e,p.freeze(),buffer);assert(n);
        auto d=decode_packet(Bytes{buffer}.first(*n));assert(d&&std::get<CountryCodeValue>(*d->fields[0].value())==code);
    }
    ContextPacket invalid;assert(!invalid.set<Bind>(2));assert(!invalid.set<CountryCode>({0x800,false}));
    assert(!invalid.set<EmsDeviceClass>({0x1000,0,false,false}));assert(!invalid.set<EmsDeviceClass>({0,3,false,false}));
    for(const auto& field:cif2_descriptors)if(cif2_generic16_field(field.id))assert(!invalid.set_value(field.id,std::uint32_t{0x10000}));
    // Every individual reserved country bit is rejected, including figure-disputed bit11.
    ContextPacket country;assert(country.set<CountryCode>({0,false}));
    auto n=encode_packet(e,country.freeze(),buffer);assert(n);
    for(unsigned bit=0;bit<32;++bit)if(0xffff7800u&(1u<<bit)) {
        auto malformed=buffer;malformed[16+3-bit/8]|=std::byte{static_cast<unsigned char>(1u<<(bit%8))};
        assert(!decode_packet(Bytes{malformed}.first(*n)));
    }
    for(auto id:{Bind::id,OperatorId::id,EmsDeviceClass::id,ModulationClass::id,ModulationType::id,
                 FunctionId::id,ModeId::id,EventId::id,FunctionPriority::id}) {
        ContextPacket p;assert(p.set_value(id,placeholder(id)));n=encode_packet(e,p.freeze(),buffer);assert(n);
        buffer[16]=std::byte{0x80};assert(!decode_packet(Bytes{buffer}.first(*n)));
    }
    ContextPacket ems;assert(ems.set<EmsDeviceClass>({0,0,false,false}));n=encode_packet(e,ems.freeze(),buffer);assert(n);
    buffer[18]=std::byte{0xc0};assert(!decode_packet(Bytes{buffer}.first(*n)));
    // Body identifiers never change prologue identity or MID.
    ControlPacket control;assert(control.set<ControlleeId>(99));assert(control.set<ControllerId>(98));
    Envelope command;command.type=PacketType::command;command.stream_id=9;
    command.command=Command{0xa90b0000,7,Identifier::short_id(2),Identifier::short_id(3)};
    n=encode_packet(command,control.freeze(),buffer);assert(n);
    auto decoded=decode_packet(Bytes{buffer}.first(*n));assert(decoded);
    assert(decoded->envelope.envelope.command->message_id==7);
    assert(decoded->envelope.envelope.command->controllee.kind==IdentifierKind::short_id);
    assert(decoded->envelope.envelope.command->controllee.words[0]==2);
    // UUID identities remain field selectors with no sixteen-byte selector body.
    QueryPacket q;assert(q.select<ControlleeUUID>());assert(q.select<ControllerUUID>());
    command.command->cam=0xa0040000;n=encode_packet(command,q.freeze(),buffer);assert(n&&*n==32);
    decoded=decode_packet(Bytes{buffer}.first(*n));assert(decoded&&decoded->fields.size()==2);
    DiagnosticAck warning,error;assert(warning.diagnostic(ControlleeUUID::id,1u<<27));assert(error.diagnostic(ControllerUUID::id,1u<<29));
    command.ack=true;command.command->cam=0xa90b0c00;
    n=encode_diagnostic(command,warning.freeze(),error.freeze(),RequestContext{0xa90b0000},buffer);assert(n);
    decoded=decode_packet(Bytes{buffer}.first(*n),DecodeOptions{RequestContext{0xa90b0000}});
    assert(decoded&&decoded->fields.size()==2&&decoded->fields[0].bytes.size()==4);
}
