#include <vita/codec/packet.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <cassert>
#include <vector>
#include <algorithm>
using namespace vita;using namespace vita::codec;
static std::vector<std::byte> words(std::initializer_list<std::uint32_t> values){std::vector<std::byte>b;for(auto w:values)for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));return b;}
static void bad(const std::vector<std::byte>& bytes){unsigned calls=0;auto p=decode_and_visit(bytes,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!p&&calls==0);}
int main(){static_assert(sizeof(SemanticValue)==16);Envelope env;env.type=PacketType::context;env.stream_id=1;
 constexpr unsigned bits[]{30,27,26,25,24,23,22,21,20};
 for(auto bit:bits){auto raw=words({0x40000006,1,8,1u<<bit,0xffffffff,0xfffffffe});auto decoded=decode_packet(raw);assert(decoded&&decoded->fields.size()==1);auto value=decoded->fields[0].value();assert(value&&std::get<Femtoseconds>(*value).count==-2);ContextPacket b;auto accepted=b.set_value({3,static_cast<std::uint8_t>(bit)},Femtoseconds{-2});assert(bool(accepted)==(bit==30||bit==25));if(accepted){std::array<std::byte,24>out{};auto encoded=encode_packet(env,b.freeze(),out);assert(encoded&&std::equal(raw.begin(),raw.end(),out.begin()));}ContextPacket positive;assert(positive.set_value({3,static_cast<std::uint8_t>(bit)},Femtoseconds{INT64_MAX}));for(std::size_t n=0;n<raw.size();++n)bad({raw.begin(),raw.begin()+n});auto policy=vita::runtime::transaction::iq_validate({3,static_cast<std::uint8_t>(bit)},*value);assert(!policy.resolvable);}
 struct Literal{unsigned bit;std::uint32_t word;SemanticValue value;};
 const Literal values[]{ {7,0x0000bbb7,CelsiusQ6{-17481}}, {6,0x00007fff,CelsiusQ6{32767}}, {5,0x0000ffff,HumidityCode{65535}}, {4,0x0001ffff,BarometricPressureCode{131071}}, {3,0x0000fd29,SeaSwellValue{9,9,63}}, {2,0x0000ffff,std::uint32_t{65535}}, {1,0xfedcba98,std::uint32_t{0xfedcba98}} };
 for(auto& c:values){auto raw=words({0x40000005,1,8,1u<<c.bit,c.word});auto p=decode_packet(raw);assert(p);auto value=p->fields[0].value();assert(value&&*value==c.value);ContextPacket builder;assert(builder.set_value({3,static_cast<std::uint8_t>(c.bit)},c.value));std::array<std::byte,20>out{};assert(encode_packet(env,builder.freeze(),out)&&std::equal(raw.begin(),raw.end(),out.begin()));}
 for(unsigned bit:{7u,6u}){auto raw=words({0x40000005,1,8,1u<<bit,0x0000bbb6});auto p=decode_packet(raw);assert(p&&std::get<CelsiusQ6>(*p->fields[0].value()).q6==-17482);ContextPacket target;assert(!p->fields[0].materialize_into(target));}
 ContextPacket invalid;assert(!invalid.set<BarometricPressure>(BarometricPressureCode{131072}));assert(!invalid.set<SeaSwellState>(SeaSwellValue{10,0,0}));assert(!invalid.set<SeaSwellState>(SeaSwellValue{0,10,0}));assert(!invalid.set<SeaSwellState>(SeaSwellValue{0,0,64}));
 // Fixed scalar reserved-bit rules and full raw range do not infer assignment databases.
 for(unsigned bit:{7u,6u,5u,3u,2u})for(unsigned r=16;r<32;++r)bad(words({0x40000005,1,8,1u<<bit,1u<<r}));for(unsigned r=17;r<32;++r)bad(words({0x40000005,1,8,1u<<4,1u<<r}));
 for(unsigned bit:{30u,27u,26u,25u,24u,23u,22u,21u,20u,17u,16u,7u,6u,5u,4u,3u,2u,1u,31u}){auto query=words({0x60000008,1,0xa0040000,9,2,3,8,1u<<bit});auto p=decode_packet(query);assert(p&&p->fields.size()==1&&p->fields[0].bytes.empty());auto diag=words({0x64000009,1,0xa90a0400,9,2,3,8,1u<<bit,0x80000000});auto d=decode_packet(diag,DecodeOptions{RequestContext{0xa90a0000}});assert(d&&d->fields.size()==1&&*d->fields[0].diagnostic()==0x80000000);}
}
