#include <vita/codec/packet.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <cassert>
using namespace vita;using namespace vita::codec;
static auto wire(unsigned bit,std::uint32_t value){std::array<std::uint32_t,5>w{0x40000005,1,4,1u<<bit,value};std::array<std::byte,20>b{};for(unsigned i=0;i<5;++i)for(unsigned j=0;j<4;++j)b[i*4+j]=std::byte((w[i]>>(24-8*j))&255);return b;}
static void bad(unsigned bit,std::uint32_t value){auto w=wire(bit,value);unsigned calls=0;auto p=decode_and_visit(w,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!p&&calls==0);}
int main(){for(bool iso:{false,true})for(std::uint16_t code:{std::uint16_t{0},std::uint16_t{1},std::uint16_t{2047}}){auto w=wire(19,(iso?0x8000u:0u)|code);auto p=decode_packet(w);assert(p);auto v=p->fields[0].value();assert(v&&std::get<CountryCodeValue>(*v)==(CountryCodeValue{code,iso}));ContextPacket b;assert(b.set<CountryCode>(CountryCodeValue{code,iso}));std::array<std::byte,20>out{};Envelope e;e.type=PacketType::context;e.stream_id=1;assert(encode_packet(e,b.freeze(),out)&&out==w);}
 for(unsigned r=11;r<32;++r)if(r!=15)bad(19,1u<<r);
 for(std::uint8_t org=0;org<3;++org)for(bool exc:{false,true})for(bool rx:{false,true}){auto w=wire(14,(std::uint32_t(org)<<14)|(exc?0x2000:0)|(rx?0x1000:0)|0xfff);auto p=decode_packet(w);assert(p);auto value=p->fields[0].value();assert(value&&std::get<EmsDeviceClassValue>(*value)==(EmsDeviceClassValue{4095,org,exc,rx}));ContextPacket b;assert(b.set<EmsDeviceClass>(EmsDeviceClassValue{4095,org,exc,rx}));std::array<std::byte,20>out{};Envelope e;e.type=PacketType::context;e.stream_id=1;assert(encode_packet(e,b.freeze(),out)&&out==w);}
 bad(14,0xc000);for(unsigned r=16;r<32;++r)bad(14,1u<<r);
 ContextPacket b;assert(b.set<CountryCode>(CountryCodeValue{42,true}));auto generation=b.freeze().generation();assert(!b.replace<CountryCode>(CountryCodeValue{2048,true}));assert(b.freeze().generation()==generation);assert(!b.set<EmsDeviceClass>(EmsDeviceClassValue{4096,0,false,false}));assert(!b.set<EmsDeviceClass>(EmsDeviceClassValue{1,3,false,false}));assert(b.freeze().generation()==generation);assert(!b.with_attributes(attribute_bit(Attribute::minimum)));
 // Full CIF2 registry exceeds the current selected-field limit; callback publication remains atomic.
 std::array<std::byte,156> all{};auto word=[&](unsigned at,std::uint32_t v){for(unsigned j=0;j<4;++j)all[at*4+j]=std::byte((v>>(24-8*j))&255);};word(0,0x40000027);word(1,1);word(2,4);word(3,0xfffffff8);unsigned calls=0;auto result=decode_and_visit(all,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!result&&calls==0&&(result.error().code==ErrorCode::resource_limit||result.error().code==ErrorCode::capacity_exhausted));
}
