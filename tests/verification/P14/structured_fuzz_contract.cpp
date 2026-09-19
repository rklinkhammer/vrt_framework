#include "../../../fuzz/packet_target.hpp"
#include <cassert>
#include <vector>
using namespace vita;
static std::vector<std::byte> words(std::initializer_list<std::uint32_t> in){std::vector<std::byte> b;for(auto w:in)for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));return b;}
static Result<void> sentence(void*,std::span<const char>)noexcept{return {};}
int main(){const std::array seeds{
 words({0x4000000e,1,1u<<14,0x00ffffff,0xffffffff,0xffffffff,0xffffffff,0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff}),
 words({0x40000010,1,1u<<11,0x00ffffff,0xffffffff,0xffffffff,0xffffffff,0,0,0,0,0,0,0,0,0}),
 words({0x40000007,1,1u<<9,0x00ffffff,2,0x24412c31,0x0d0a0000}),
 words({0x4000000a,1,1u<<8,0x00010001,0x00018001,1,2,3,4,5})};
 std::uint64_t state=0x17a49b00d;auto random=[&](){state^=state<<13;state^=state>>7;state^=state<<17;return state;};
 for(auto& seed:seeds){assert(codec::decode_packet(seed));fuzz::exercise(seed);}
 for(unsigned n=0;n<20000;++n){auto input=seeds[random()%seeds.size()];const auto edits=1+random()%8;for(unsigned k=0;k<edits;++k){switch(random()%3){case 0:input[random()%input.size()]=std::byte(random()&255);break;case 1:if(input.size()>4)input.resize(4+random()%(input.size()-3));break;default:if(input.size()<128)input.push_back(std::byte(random()&255));break;}}
  fuzz::exercise(input);auto p=codec::decode_packet(input);if(p){NativeContextPacket<> materialized;for(std::size_t i=0;i<p->fields.size();++i){auto& f=p->fields[i];if(f.kind==BodyKind::values){if(f.id==FormattedGPS::id||f.id==FormattedINS::id)(void)f.geolocation();if(f.id==ECEFEphemeris::id||f.id==RelativeEphemeris::id)(void)f.ephemeris();if(f.id==GPSASCII::id)(void)f.gps_ascii();if(f.id==ContextAssociationLists::id)(void)f.associations();(void)f.materialize_into(materialized,{nullptr,sentence});}}}
 }
}
