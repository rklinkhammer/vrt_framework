#include <vita/codec/array_cif.hpp>
#include <array>
#include <cassert>
#include <cstdio>
#include <type_traits>
using namespace vita;
namespace ac=vita::codec::array_cif;
constexpr ac::Options opts{ac::Dialect::i9_five_cifs_header7,{}};
static_assert(!std::is_default_constructible_v<ac::View>);
static_assert(!std::is_default_constructible_v<ac::Options>);
struct Wire {
 std::array<std::byte,8192> bytes{};std::size_t n=0;
 void put(std::size_t i,std::uint32_t x){for(unsigned j=0;j<4;++j)bytes[4*i+j]=std::byte(x>>(24-j*8));}
 std::uint32_t get(std::size_t i)const{std::uint32_t x=0;for(unsigned j=0;j<4;++j)x=(x<<8)|std::to_integer<unsigned>(bytes[4*i+j]);return x;}
 Bytes view()const{return Bytes(bytes).first(n*4);}
};
Wire leaf(unsigned count){Wire w;w.n=8+count*2;w.put(0,w.n);w.put(1,0x07002000|count);w.put(3,0x40000000);for(unsigned i=0;i<count;++i){w.put(8+2*i,17);w.put(9+2*i,0x12340000+i);}return w;}
Wire parent(const Wire& c){Wire w;w.n=9+c.n;w.put(0,w.n);w.put(1,0x07000001|((c.n+1)<<12));w.put(4,0x800);w.put(8,71);for(std::size_t i=0;i<c.n;++i)w.put(9+i,c.get(i));return w;}
// Independent restricted-schema oracle: RefPoint and nested arrays only, implicitCurrent.
// It reads raw integer words and does not call production descriptors or layout helpers.
struct Counts {std::size_t fields=1,views=0,work=0;};
bool oracle(const Wire&w,std::size_t start,std::size_t available,unsigned depth,Counts& c,const ac::Budget& b,std::size_t& used){
 if(available<8)return false;
 auto total=w.get(start),h=w.get(start+1);unsigned width=(h>>12)&4095,count=h&4095;
 if(h>>24!=7||w.get(start+2)||total!=8+width*count||total>available||(count&&!width))return false;
 auto m0=w.get(start+3),m1=w.get(start+4);
 if(m0&~0xc000000eu||m1&~0x800u||w.get(start+5)||w.get(start+6)||w.get(start+7))return false;
 if(depth>4||depth>b.max_depth||count>b.max_records)return false;
 ++c.work;
 for(unsigned i=0;i<count;++i){std::size_t at=start+8+i*width+1,end=start+8+(i+1)*width;++c.work;
  if(m0&0x40000000){++c.fields;++c.views;++c.work;if(at>=end)return false;++at;}
  if(m1&0x800){++c.fields;++c.views;std::size_t child=0;if(at>end||!oracle(w,at,end-at,depth+1,c,b,child))return false;at+=child;}
  if(at!=end)return false;
 }
 used=total;return c.fields<=b.max_fields&&c.views<=b.max_views&&c.work<=b.max_work;
}
int main(){
 auto base=leaf(2);ac::Budget b;auto v=ac::validate(base.view(),opts,b);assert(v&&b.fields==3&&b.views==2&&b.work==5);
 unsigned calls=0;assert(v->visit([&](const ac::Element&e)noexcept->Result<void>{assert(e.path.depth==1&&e.path.steps[0].index==17&&e.path.steps[0].ordinal==calls);assert(e.field.id==ReferencePoint::id);++calls;return {};}));assert(calls==2);assert(!v->record(2));
 calls=0;assert(!v->visit([&](const ac::Element&)noexcept->Result<void>{++calls;return std::unexpected(Error{ErrorCode::callback_failure});}));assert(calls==1);
 // Every proper byte truncation rejects before a View/callback can be obtained.
 for(std::size_t size=0;size<base.view().size();++size){ac::Budget q;q.fields=3;q.views=7;q.work=11;auto r=ac::validate(base.view().first(size),opts,q);assert(!r&&q.fields==3&&q.views==7&&q.work==11);}
 // Inherited Age widths, including fractional-only and invalid absent representation.
 for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf){Wire w;unsigned width=1+(tsi?1:0)+(tsf?2:0);w.n=8+width;w.put(0,w.n);w.put(1,0x07000001|(width<<12));w.put(6,1u<<17);w.put(8,5);ac::Budget q;auto r=ac::validate(w.view(),{ac::Dialect::i9_five_cifs_header7,{static_cast<std::uint8_t>(tsi),static_cast<std::uint8_t>(tsf),true}},q);assert(bool(r)==bool(tsi||tsf));}
 // Explicit Current and unsupported registered CIF7 combinations.
 auto explicit_current=base;explicit_current.put(3,0x40000080);explicit_current.put(7,0x80000000);b={};assert(ac::validate(explicit_current.view(),opts,b));
 explicit_current.put(7,0);b={};auto r=ac::validate(explicit_current.view(),opts,b);assert(!r&&r.error().code==ErrorCode::unsupported_capability);
 explicit_current.put(3,0x40000000);explicit_current.put(7,0x80000000);b={};r=ac::validate(explicit_current.view(),opts,b);assert(!r&&r.error().code==ErrorCode::unsupported_capability);
 // Exact default field/depth boundaries and shared cursor across sibling calls.
 auto maximum=leaf(127);b={};assert(ac::validate(maximum.view(),opts,b)&&b.fields==128);
 maximum=leaf(128);b={};assert(!ac::validate(maximum.view(),opts,b));
 auto nested=parent(parent(parent(leaf(1))));b={};auto nested_view=ac::validate(nested.view(),opts,b);assert(nested_view);
 unsigned depth_calls=0;assert(nested_view->visit([&](const ac::Element&e)noexcept->Result<void>{assert(e.path.depth==4-depth_calls);assert(e.path.steps[0].index==71);++depth_calls;return {};}));assert(depth_calls==4);
 nested=parent(nested);b={};assert(!ac::validate(nested.view(),opts,b));
 auto one=leaf(1);b={};b.work=4093;assert(ac::validate(one.view(),opts,b)&&b.work==4096);assert(!ac::validate(one.view(),opts,b)&&b.work==4096);
 // Empty records still charge record units and obey record count ceiling.
 Wire empty;empty.n=8+256;empty.put(0,empty.n);empty.put(1,0x07001100);b={};assert(ac::validate(empty.view(),opts,b)&&b.work==257);
 ++empty.n;empty.put(0,empty.n);empty.put(1,0x07001101);b={};assert(!ac::validate(empty.view(),opts,b));
 // Live CIF1 Index List extent metadata reaches the shadow resolver unchanged.
 Wire index;index.n=12;index.put(0,12);index.put(1,0x07004001);index.put(4,0x80);index.put(8,3);index.put(9,3);index.put(10,0x10000001);index.put(11,0x55000000);
 b={};b.field_limits.index_entries=0;assert(!ac::validate(index.view(),opts,b));b.field_limits.index_entries=1;assert(ac::validate(index.view(),opts,b));
 index.put(11,0x55000001);b={};assert(!ac::validate(index.view(),opts,b));
 // Native construction/semantic use and emission are intentionally absent.
 // Independent exact resource oracle across deterministic nested extent/mask mutations.
 std::array<Wire,4> seeds{leaf(0),leaf(2),parent(leaf(1)),parent(parent(leaf(2)))};
 std::uint32_t rng=0x49c1f7;unsigned accepted=0,rejected=0;
 for(unsigned i=0;i<24000;++i){auto w=seeds[i%4];rng=rng*1664525u+1013904223u;unsigned mode=(i/4)%8;
  if(mode==1)w.put(0,w.get(0)+(rng%3));
  if(mode==2)w.put(1,w.get(1)^(1u<<(rng%24)));
  if(mode==3)w.put(3,w.get(3)^(1u<<(rng%32)));
  if(mode==4&&w.n>17)w.put(9,w.get(9)^(1u<<(rng%8)));
  if(mode==5&&w.n>17)w.put(10,w.get(10)^(1u<<(rng%24)));
  if(mode==6)w.put(2,rng&1);
  b={};b.max_fields=1+rng%14;b.max_views=rng%12;b.max_work=1+rng%20;b.max_depth=1+rng%4;
  Counts count;std::size_t used=0;bool expected=oracle(w,0,w.n,1,count,b,used)&&used==w.n;
  auto result=ac::validate(w.view(),opts,b);if(bool(result)!=expected){std::fprintf(stderr,"oracle mismatch case%u mode%u\n",i,mode);return 1;}
  if(result){++accepted;assert(b.fields==count.fields&&b.views==count.views&&b.work==count.work);unsigned seen=0;assert(result->visit([&](const ac::Element&)noexcept->Result<void>{++seen;return {};}));assert(seen==count.views);}else{++rejected;assert(b.fields==0&&b.views==0&&b.work==0);}
 }
 // Shared resolver default retains ordinary framing and scalar descriptor behavior.
 const std::array<std::byte,16> packet{std::byte{0x40},{},{},std::byte{4},{},{},{},std::byte{1},std::byte{0x40},{},{},{},{},{},{},std::byte{9}};
 assert(vita::codec::decode_packet(packet));
 std::printf("24000 mutation cases: %u accepted, %u rejected\n",accepted,rejected);
}
