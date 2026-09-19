#include <vita/codec/prologue.hpp>
#include <array>
#include <cassert>
#include <cstring>
using namespace vita;
using namespace vita::codec;
int main() {
    Envelope e; e.type=PacketType::signal; e.stream_id=0x11223344; e.packet_count=3;
    e.class_id=ClassId{0x123456,0x789a,0xbcde,0}; e.timestamp={Tsi::gps,Tsf::picoseconds,0x55667788,0x0000000102030405ULL}; e.trailer=true;
    auto layout=measure_prologue(e,1024); assert(layout && layout->packet_bytes==1056 && layout->prologue_bytes==28 && *layout->trailer_offset==1052);
    std::array<std::byte,64> prefix; prefix.fill(std::byte{0xa5});
    auto encoded=encode_prologue(e,1024,prefix); assert(encoded && *encoded==28);
    constexpr std::array<unsigned char,28> literal={0x1c,0xa3,0x01,0x08,0x11,0x22,0x33,0x44,0x00,0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0x55,0x66,0x77,0x88,0,0,0,1,2,3,4,5};
    assert(std::memcmp(prefix.data(),literal.data(),literal.size())==0);
    for(std::size_t i=28;i<prefix.size();++i) assert(prefix[i]==std::byte{0xa5});
    prefix.fill(std::byte{0x5a}); auto short_out=encode_prologue(e,1024,MutableBytes(prefix).first(27));
    assert(!short_out && short_out.error().required_capacity==28); for(auto b:prefix) assert(b==std::byte{0x5a});
    assert(!encode_prologue(e,262140,prefix)); for(auto b:prefix) assert(b==std::byte{0x5a});
    auto trailer=encode_trailer(0x12345678,prefix); assert(trailer && *trailer==4 && prefix[0]==std::byte{0x12} && prefix[3]==std::byte{0x78} && prefix[4]==std::byte{0x5a});
    prefix.fill(std::byte{0x5a}); assert(!encode_trailer(0,MutableBytes(prefix).first(3))); for(auto b:prefix) assert(b==std::byte{0x5a});
    for(unsigned type=0;type<8;++type) for(unsigned tsi=0;tsi<4;++tsi) for(unsigned tsf=0;tsf<4;++tsf) {
        Envelope option; option.type=static_cast<PacketType>(type); if(type!=0 && type!=2) option.stream_id=1;
        option.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),1,2}; option.class_id=ClassId{1,2,3,0};
        if(type>=6) option.command=Command{};
        auto size=measure_prologue(option,4); assert(size); auto header=encode_prologue(option,4,prefix); assert(header && *header==size->prologue_bytes);
        std::array<std::byte,128> whole{}; std::array<std::byte,4> payload{}; auto full=encode_envelope(option,payload,{},whole); assert(full);
        assert(std::memcmp(prefix.data(),whole.data(),size->prologue_bytes)==0);
    }
}
