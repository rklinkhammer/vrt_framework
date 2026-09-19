#include <vita/codec/packet.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <algorithm>
#include <cassert>
#include <vector>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::codec;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void* p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void* p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void* p,std::align_val_t)noexcept{std::free(p);}
static std::vector<std::byte> words(std::initializer_list<std::uint32_t> in){std::vector<std::byte> b;for(auto w:in)for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));return b;}
struct Case{unsigned bit;SemanticValue value;std::vector<std::byte> bytes;};
int main(){static_assert(sizeof(SemanticValue)==16);const std::array cases{
 Case{31,RadiansQ7{-128},words({0x0000ff80})},
 Case{30,PolarizationAngles{-128,64},words({0xff800040})},
 Case{29,PointingAngles{65535,-11520},words({0xd300ffff})},
 Case{27,std::uint32_t{65535},words({0x0000ffff})},
 Case{26,SpatialReferenceValue{0xabcd,3,2},words({0xabcd000e})},
 Case{25,BeamWidthCode{0xb400,0xffff},words({0xb400ffff})},
 Case{24,MetresQ6{UINT32_MAX},words({0xffffffff})},
 Case{20,EbNoBerValue{std::nullopt,-128},words({0x7fffff80})},
 Case{19,ThresholdValue{-128,0},words({0x0000ff80})},
 Case{18,DecibelsQ7{-128},words({0x0000ff80})},
 Case{17,InterceptPointsValue{std::nullopt,-32768},words({0x7fff8000})},
 Case{16,SnrNoiseFigureValue{-128,std::nullopt},words({0xff800000})},
 Case{15,Hertz{-1},words({0xffffffff,0xffffffff})},
 Case{14,GainStages{-32768,32767},words({0x7fff8000})},
 Case{13,Hertz{INT64_MAX},words({0x7fffffff,0xffffffff})},
 Case{6,std::uint32_t{0xa55aff00},words({0xa55aff00})},
 Case{5,Unsigned64Bits{0x12345678fedcba98ull},words({0x12345678,0xfedcba98})},
 Case{4,std::uint32_t{65535},words({0x0000ffff})},
 Case{3,std::uint32_t{4},words({4})},
 Case{2,VersionBuildValue{127,366,63,1023},words({0xff6effff})},
 Case{1,BufferSizeValue{UINT32_MAX,0xa5,0x5a},words({0xffffffff,0x0000a55a})}
 };
 for(auto& c:cases){const FieldId id{1,static_cast<std::uint8_t>(c.bit)};auto wire=words({0x40000000u|static_cast<unsigned>(4+c.bytes.size()/4),1,2,1u<<c.bit});wire.insert(wire.end(),c.bytes.begin(),c.bytes.end());auto parsed=decode_packet(wire);assert(parsed&&parsed->fields.size()==1&&parsed->fields[0].id==id);auto value=parsed->fields[0].value();assert(value&&*value==c.value);ContextPacket builder;assert(builder.set_value(id,c.value));Envelope e;e.type=PacketType::context;e.stream_id=1;std::array<std::byte,128> out{};auto encoded=encode_packet(e,builder.freeze(),out);assert(encoded&&*encoded==wire.size()&&std::equal(wire.begin(),wire.end(),out.begin()));
  for(std::size_t n=0;n<wire.size();++n){unsigned callbacks=0;auto r=decode_and_visit(Bytes{wire}.first(n),{},[&](FieldView)noexcept->Result<void>{++callbacks;return {};});assert(!r&&callbacks==0);}out.fill(std::byte{0xa5});assert(!encode_packet(e,builder.freeze(),MutableBytes{out}.first(wire.size()-1)));assert(std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0xa5};}));
  auto qwire=words({0x60000008,1,0xa0040000,9,2,3,2,1u<<c.bit});auto q=decode_packet(qwire);assert(q&&q->fields.size()==1&&q->fields[0].bytes.empty()&&q->fields[0].kind==BodyKind::selectors);QueryPacket query;assert(query.select(id));assert(query.with_attributes(attribute_bit(Attribute::minimum)));assert(!query.with_attributes(1u<<18));
  auto d=words({0x64000009,1,0xa90a0400,9,2,3,2,1u<<c.bit,0x80000000});auto diag=decode_packet(d,DecodeOptions{RequestContext{0xa90a0000}});assert(diag&&diag->fields.size()==1&&diag->fields[0].bytes.size()==4&&*diag->fields[0].diagnostic()==0x80000000);
  auto policy=vita::runtime::transaction::iq_validate(id,c.value);assert(!policy.resolvable&&(policy.diagnostics.errors&vita::runtime::transaction::unsupported));
 }
 // The21 individually supported fields still exceed the unchanged16-field decode policy.
 std::uint32_t full_mask=0;std::size_t full_words=4;for(auto& c:cases){full_mask|=1u<<c.bit;full_words+=c.bytes.size()/4;}auto over=words({0x40000000u|static_cast<unsigned>(full_words),1,2,full_mask});for(auto& c:cases)over.insert(over.end(),c.bytes.begin(),c.bytes.end());unsigned calls=0;auto limited=decode_and_visit(over,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!limited&&calls==0&&(limited.error().code==ErrorCode::capacity_exhausted||limited.error().code==ErrorCode::resource_limit));
 // Insertion order must not become CIF traversal order across scalar families.
 ContextPacket mixed;assert(mixed.set<BufferSize>(BufferSizeValue{7,1,2}));assert(mixed.set<PhaseOffset>(RadiansQ7{1}));assert(mixed.set<SampleRate>(Hertz{1048576}));auto literal=words({0x40000009,1,0x00200002,0x80000002,0,0x00100000,1,7,0x00000102});std::array<std::byte,128> out{};Envelope e;e.type=PacketType::context;e.stream_id=1;auto encoded=encode_packet(e,mixed.freeze(),out);assert(encoded&&*encoded==literal.size()&&std::equal(literal.begin(),literal.end(),out.begin()));
 const auto count_before=allocations;
 for(unsigned iteration=0;iteration<1000;++iteration)for(auto& c:cases){ContextPacket scalar;assert(scalar.set_value({1,static_cast<std::uint8_t>(c.bit)},c.value));auto snapshot=scalar.freeze();assert(measure(snapshot));std::array<std::byte,64> bytes{};auto size=encode_packet(e,snapshot,bytes);assert(size);auto parsed=decode_packet(Bytes{bytes}.first(*size));assert(parsed&&parsed->fields[0].value());}
 assert(allocations==count_before);auto* a=::operator new(32);auto* b=::operator new(64,std::align_val_t{64});assert(allocations==count_before+2);::operator delete(a);::operator delete(b,std::align_val_t{64});

}
