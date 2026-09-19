#include <vita/codec/segmented_samples.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
#include <limits>
using namespace vita;
namespace S=vita::codec::samples;
namespace G=vita::codec::general;
static bool counting=false;static unsigned allocations=0;
void* operator new(std::size_t n){if(counting)++allocations;auto p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){if(counting)++allocations;void*p=nullptr;if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))std::abort();return p;}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}
static void write(std::span<std::byte>b,std::size_t offset,unsigned width,std::uint64_t value){for(unsigned i=0;i<width;++i){auto mask=std::byte(128u>>((offset+i)%8));if((value>>(width-1-i))&1)b[(offset+i)/8]|=mask;else b[(offset+i)/8]&=~mask;}}
int main(){
 counting=true;auto p=::operator new(3);::operator delete(p);p=::operator new(64,std::align_val_t{64});::operator delete(p,std::align_val_t{64});counting=false;assert(allocations==2);allocations=0;counting=true;
 // Three-bit data, four deliberately nonzero unused bits, two event bits, three channel bits.
 // Vector2 x component-repeat3 x complex2 gives twelve fields in each complete structure.
 for(bool processing:{false,true})for(unsigned kind:{1u,2u})for(bool repeat_component:{false,true}){
   const std::uint32_t high=(processing?0u:0x80000000u)|(kind<<29)|(repeat_component?0x00800000:0)|0x00230000|(11u<<6)|2;
   auto d=S::descriptor({(std::uint64_t(high)<<32)|0x00020001},S::SignalDomain::time);assert(d);
   constexpr unsigned count=12;const unsigned extent=processing?24:20;
   std::array<std::byte,26> buffer{};buffer.fill(std::byte{0xff});auto payload=MutableBytes(buffer).subspan(1,extent);
   std::array<G::Item,count> expected{};
   for(unsigned i=0;i<count;++i){const unsigned bit=processing?(i/2)*32+(i%2)*12:i*12;expected[i]={i%8,(i*3)%8,(i+1)%4};write(payload,bit,3,expected[i].data_bits);write(payload,bit+7,2,expected[i].event_tag);write(payload,bit+9,3,expected[i].channel_tag);}
   S::PayloadBinding binding{1,{S::PadReporting::exact,std::uint8_t(processing?8:16)}};
   auto contiguous=S::contiguous(payload,*d,binding);assert(contiguous);
   std::array<Bytes,24> pieces{};for(unsigned i=0;i<extent;++i)pieces[i]=Bytes(payload).subspan(i,1);
   auto fragmented=S::SegmentedSamples<24>::create(pieces,*d,binding);assert(fragmented);pieces={};
   auto copied=*fragmented;for(unsigned i=0;i<count;++i){assert(copied.at(i)==expected[i]);assert(contiguous->at(i)==expected[i]);}
   assert(!copied.at(count));assert(!S::contiguous(Bytes(payload).first(extent-1),*d,binding));
   assert(!S::contiguous(Bytes(buffer).first(extent+1),*d,binding));
   assert(!S::measure_payload(*d,binding,{11,100}));assert(!S::measure_payload(*d,binding,{12,extent-1}));
   auto bad=binding;bad.padding.policy=static_cast<S::PadReporting>(99);assert(!S::measure_payload(*d,bad));
   bad=binding;bad.padding.class_id_pad_bits=std::nullopt;assert(!S::measure_payload(*d,bad));
   bad=binding;bad.structures=std::numeric_limits<std::size_t>::max();assert(!S::measure_payload(*d,bad));
 }
 for(unsigned code:{0u,16u,1u,17u,7u,23u,13u,14u,15u}){
   unsigned width=code==13?16:code==14?32:code==15?64:8;
   const std::uint32_t high=0xc0000000u|(code<<24)|((width-1)<<6)|(width-1);
   auto d=S::descriptor({std::uint64_t(high)<<32},S::SignalDomain::spectral);assert(d);
   auto unit=S::component_unit(*d,1);assert(unit);
   auto expected=code>=13&&code<=15?S::ComponentUnit::radians:code==7||code==23?S::ComponentUnit::unspecified:code<16?S::ComponentUnit::pi_multiple:S::ComponentUnit::two_pi_multiple;
   assert(*unit==expected);assert(S::component_unit(*d,0)==S::ComponentUnit::dimensionless);assert(!S::component_unit(*d,2));
 }
 counting=false;assert(allocations==0);
}
