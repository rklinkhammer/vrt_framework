#include <vita/codec/packet.hpp>
#include <array>
#include <algorithm>
#include <cassert>
using namespace vita;using namespace vita::codec;
template<std::size_t N>static auto bytes(const std::array<std::uint32_t,N>& words){std::array<std::byte,N*4> b{};for(std::size_t i=0;i<N;++i)for(unsigned j=0;j<4;++j)b[i*4+j]=std::byte(words[i]>>(24-8*j));return b;}
int main(){
 // Independent fixed Q20 words: no production encoder establishes the expected values.
 struct Vector{std::uint64_t hz;std::uint32_t high,low;};
 constexpr std::array vectors{Vector{1'000'000,0xf4,0x24000000},Vector{100'000'000,0x5f5e,0x10000000},Vector{100'050'000,0x5f6a,0x45000000},Vector{100'200'000,0x5f8e,0xe4000000},Vector{6'000'000'000,0x165a0b,0xc0000000}};
 for(auto v:vectors){
   auto expected=bytes(std::array<std::uint32_t,11>{0x6800000b,1,0x00abcdef,0x00020120,0xa91c0000,0x11223344,2,3,0x08000000,v.high,v.low});
   auto decoded=decode_packet(expected);assert(decoded&&decoded->fields.size()==1);assert(decoded->fields[0].id==RFReferenceFrequency::id);assert(std::get<Hertz>(*decoded->fields[0].value()).q20==static_cast<std::int64_t>(v.hz*1'048'576));
   ControlPacket command;assert(command.configure(0x120,2));assert(command.set<RFReferenceFrequency>(Hertz{static_cast<std::int64_t>(v.hz*1'048'576)}));Envelope envelope;envelope.type=PacketType::command;envelope.stream_id=1;envelope.class_id=ClassId{0xabcdef,2,0x120};envelope.command=Command{0xa91c0000,0x11223344,Identifier::short_id(2),Identifier::short_id(3)};
   std::array<std::byte,44> encoded{};auto n=encode_packet(envelope,command.freeze(),encoded);assert(n&&*n==expected.size()&&encoded==expected);
   for(std::size_t length=0;length<expected.size();++length){unsigned calls=0;auto truncated=decode_and_visit(Bytes{expected}.first(length),{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!truncated&&calls==0);}
 }
 auto query=bytes(std::array<std::uint32_t,9>{0x68000009,1,0xabcdef,0x00020120,0xa0040000,0x11223344,2,3,0x08000000});auto selection=decode_packet(query);assert(selection&&selection->fields.size()==1&&selection->fields[0].id==RFReferenceFrequency::id&&selection->fields[0].kind==BodyKind::selectors&&selection->fields[0].bytes.empty());
 // Wire order is ReferencePoint, RF, SampleRate, StateEvent, DPF; RF is not last.
 auto context=bytes(std::array<std::uint32_t,16>{0x48a00010,1,0xabcdef,0x00020110,1,0,0,0x48218000,1,0x5f5e,0x10000000,0x18,0x6a000000,0,0xa00003cf,0});auto parsed=decode_packet(context);assert(parsed&&parsed->fields.size()==5);constexpr std::array ids{ReferencePoint::id,RFReferenceFrequency::id,SampleRate::id,StateEvent::id,DataPayloadFormat::id};for(unsigned i=0;i<ids.size();++i)assert(parsed->fields[i].id==ids[i]);assert(std::get<Hertz>(*parsed->fields[1].value()).q20==100'000'000ll*1'048'576);assert(std::get<Hertz>(*parsed->fields[2].value()).q20==100'000ll*1'048'576);
 // The codec is intentionally profile-neutral: class authorization belongs to routing.
 auto v1=context;v1[14]=std::byte{0};v1[15]=std::byte{0x10};v1[13]=std::byte{1};assert(decode_packet(v1));
}
