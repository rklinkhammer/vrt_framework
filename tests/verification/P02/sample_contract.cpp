#include <vita/codec/samples.hpp>
#include <vita/codec/packet.hpp>
#include "vectors.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <new>
using namespace vita;using namespace vita::codec;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void* p=std::malloc(n))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
unsigned other_translation_unit() noexcept;
int main(){
    const auto before=allocations;
    std::array<Iq<std::int16_t>,2> pairs{{{16384,0},{15137,6270}}};
    std::array<std::byte,33> scratch{};
    auto packed=pack_iq16(pairs,MutableBytes{scratch}.subspan(1));
    if(!packed || *packed!=8 || !std::equal(w5.begin()+8,w5.end(),scratch.begin()+1))return 1;
    auto view=SampleView<std::int16_t>::create(Bytes{scratch}.subspan(1,8));
    if(!view || view->size()!=2 || *view->at(0)!=pairs[0] || *view->at(1)!=pairs[1] || view->at(2))return 2;
    const auto integer_golden=wire_bytes(std::array<std::uint32_t,4>{0x80000000,0x7fffffff,0xffffffff,0});
    std::array<Iq<std::int32_t>,2> ints{{{std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()},{-1,0}}};
    packed=pack_iq32(ints,scratch);if(!packed || *packed!=16 || !std::equal(integer_golden.begin(),integer_golden.end(),scratch.begin()))return 3;
    const auto float_golden=wire_bytes(std::array<std::uint32_t,4>{0x3f800000,0xbf000000,0x80000000,0x7f800000});
    std::array<Iq<float>,2> floats{{{1.0f,-0.5f},{-0.0f,std::numeric_limits<float>::infinity()}}};
    packed=pack_iq_float(floats,scratch);if(!packed || !std::equal(float_golden.begin(),float_golden.end(),scratch.begin()))return 4;
    auto fv=SampleView<float>::create(Bytes{scratch}.first(16));
    if(!fv || std::bit_cast<std::uint32_t>(fv->at(1)->i)!=0x80000000)return 5;
    if(SampleView<std::int16_t>::create(Bytes{scratch}.first(3)) || SampleView<float>::create(Bytes{scratch}.first(12)))return 6;
    scratch.fill(std::byte{0xaa});auto short_out=pack_iq32(ints,MutableBytes{scratch}.first(8));
    if(short_out || short_out.error().required_capacity!=16 || scratch[0]!=std::byte{0xaa})return 7;
    auto alias=pack_iq16(pairs,MutableBytes{reinterpret_cast<std::byte*>(pairs.data()),sizeof(pairs)});
    if(alias || pairs[0]!=Iq<std::int16_t>{16384,0})return 8;
    for(unsigned n=0;n<1000;++n){auto decoded=decode_packet(w2);if(!decoded || decoded->fields.size()!=1)return 9;}
    if(allocations!=before || other_translation_unit()!=8)return 10;
    return 0;
}
