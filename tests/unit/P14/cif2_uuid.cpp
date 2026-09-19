#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
int main() {
    UuidValue input{{0x00112233,0x44556677,0x8899aabb,0xccddeeff}};
    NativeContextPacket<32> owner;assert(owner.set<ControlleeUUID>(input));assert(owner.set<ControllerUUID>(input));
    auto snapshot=owner.freeze();auto copy=snapshot;input.words[0]=0;
    assert(snapshot.get<ControlleeUUID>()->words[0]==0x00112233);
    assert(owner.replace<ControlleeUUID>(input));assert(snapshot.get<ControlleeUUID>()->words[0]==0x00112233);
    for(unsigned i=0;i<100;++i){input.words[0]=i;assert(owner.replace<ControlleeUUID>(input));}
    assert(owner.freeze().native_size()==32);
    for(const auto& field:snapshot.fields()) {
        assert(!owner.set_value(field.id,*field.values[0]));
        assert(!copy.native_value(*field.values[0]));
    }
    assert(owner.remove<ControllerUUID>());assert(owner.freeze().native_size()==16);
    assert(owner.set<ControllerUUID>(input));
    NativeContextPacket<15> small;assert(!small.set<ControllerUUID>(input));assert(small.freeze().native_size()==0);
    std::array<std::byte,128> storage{};Envelope e;e.type=PacketType::context;e.stream_id=1;
    auto n=encode_packet(e,snapshot,storage);assert(n&&*n==48);
    const std::array<unsigned char,16> literal{0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    for(unsigned field=0;field<2;++field)for(unsigned byte=0;byte<16;++byte)assert(storage[16+field*16+byte]==std::byte{literal[byte]});
    auto decoded=decode_packet(Bytes{storage}.first(*n));assert(decoded&&decoded->fields.size()==2);
    assert(*decoded->fields[0].uuid()==*snapshot.get<ControlleeUUID>());
    assert(!decoded->fields[0].value());
    NativeContextPacket<32> materialized;
    for(std::size_t i=0;i<decoded->fields.size();++i)assert(decoded->fields[i].materialize_into(materialized));
    auto materialized_snapshot=materialized.freeze();
    assert(*materialized_snapshot.get<ControllerUUID>()==*snapshot.get<ControllerUUID>());
    for(std::size_t size=0;size<*n;++size){unsigned calls=0;assert(!decode_and_visit(Bytes{storage}.first(size),{},[&](const FieldView&) noexcept -> Result<void>{++calls;return {};}));assert(!calls);}
    auto limit=decode_packet(Bytes{storage}.first(*n),DecodeOptions{{},{},{1024,7}});assert(!limit&&limit.error().code==ErrorCode::resource_limit);
    assert(decode_packet(Bytes{storage}.first(*n),DecodeOptions{{},{},{1024,8}}));
    assert(layout_signature(snapshot)==layout_signature(owner.freeze()));
    NativeContextPacket<32> zero;
    assert(!zero.set<ControlleeUUID>({}));assert(!zero.set<ControllerUUID>({}));
    for(std::size_t byte=16;byte<48;++byte)storage[byte]=std::byte{};
    auto raw_zero=decode_packet(Bytes{storage}.first(*n));assert(raw_zero);
    assert((raw_zero->fields[0].uuid()->words==std::array<std::uint32_t,4>{}));
    assert(!raw_zero->fields[0].materialize_into(zero));assert(zero.freeze().native_size()==0);
}
