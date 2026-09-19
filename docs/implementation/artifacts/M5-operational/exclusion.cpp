#include <vita/codec/array_cif.hpp>
#include <array>
#include <cassert>
#include <cstdio>
using namespace vita;
using namespace vita::codec;
template<std::size_t N> auto words(const std::array<std::uint32_t,N>& w){std::array<std::byte,N*4>b{};for(std::size_t i=0;i<N;++i)for(unsigned j=0;j<4;++j)b[i*4+j]=std::byte(w[i]>>(24-8*j));return b;}
static void excluded(Bytes b){auto a=decode_packet(b);auto g=decode_packet_bounded<128,1664>(b);assert(!a&&!g&&a.error().code==ErrorCode::unsupported_layout&&g.error().code==ErrorCode::unsupported_layout);unsigned calls=0;auto r=decode_and_visit_bounded<128,1664>(b,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!r&&calls==0);}
int main(){
 assert(!descriptor({1,11}));
 // Valid opt-in I9 field: one record containing index7 and ReferencePoint9.
 auto array=words(std::array<std::uint32_t,10>{10,0x07002001,0,0x40000000,0,0,0,0,7,9});array_cif::Budget budget;auto optional=array_cif::validate(array,{array_cif::Dialect::i9_five_cifs_header7,{}},budget);assert(optional);
 auto context=words(std::array<std::uint32_t,14>{0x4000000e,1,2,0x800,10,0x07002001,0,0x40000000,0,0,0,0,7,9});excluded(context);
 auto control=words(std::array<std::uint32_t,18>{0x60000012,1,0xa9080000,1,2,3,2,0x800,10,0x07002001,0,0x40000000,0,0,0,0,7,9});excluded(control);
 auto query=words(std::array<std::uint32_t,8>{0x60000008,1,0xa0040000,1,2,3,2,0x800});excluded(query);
 ContextPacket native;auto set=native.set_value({1,11},SemanticValue{std::uint32_t{0}});assert(!set&&native.freeze().fields().empty());
 std::puts("Array exclusion PASS: opt-in structural valid, descriptor absent, Context/Control/query rejected at16/64 and128/1664, zero callbacks, native insertion rejected");
}
