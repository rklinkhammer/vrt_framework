#include <vita/codec/packet.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <algorithm>
#include <cassert>
#include <vector>
using namespace vita; using namespace vita::codec;
static std::vector<std::byte> words(std::initializer_list<std::uint32_t> values){std::vector<std::byte> r;for(auto w:values)for(int n=3;n>=0;--n)r.push_back(std::byte((w>>(8*n))&255));return r;}
static std::vector<std::byte> packet(unsigned bit,std::uint32_t value){return words({0x40000005,1,4,1u<<bit,value});}
static void rejected(const std::vector<std::byte>& bytes){unsigned calls=0;auto result=decode_and_visit(bytes,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!result&&calls==0);}
int main(){static_assert(sizeof(SemanticValue)==16);
 // Opaque32 and Generic16 expectations derive from the individual source clauses.
 constexpr unsigned u32bits[]{30,29,28,27,26,25,23,21,20,17,16,15,13,12,5,4,3};
 constexpr unsigned g16bits[]{18,11,10,9,8,7,6};
 auto check=[&](unsigned bit,std::uint32_t value){auto literal=packet(bit,value);auto p=decode_packet(literal);assert(p&&p->fields.size()==1);auto v=p->fields[0].value();assert(v&&std::get<std::uint32_t>(*v)==value);ContextPacket b;assert(b.set_value({2,static_cast<std::uint8_t>(bit)},value));Envelope e;e.type=PacketType::context;e.stream_id=1;std::array<std::byte,64> out{};auto n=encode_packet(e,b.freeze(),out);assert(n&&*n==literal.size()&&std::equal(literal.begin(),literal.end(),out.begin()));for(std::size_t length=0;length<literal.size();++length)rejected({literal.begin(),literal.begin()+length});out.fill(std::byte{0x7e});assert(!encode_packet(e,b.freeze(),MutableBytes{out}.first(literal.size()-1)));assert(std::all_of(out.begin(),out.end(),[](auto x){return x==std::byte{0x7e};}));p->envelope.envelope.timestamp.tsi=Tsi::gps;p->envelope.envelope.timestamp.tsf=Tsf::picoseconds;vita::runtime::context::ReceiverHistory<> history;auto received=history.receive(*p,1,{0});assert(!received&&received.error().code==ErrorCode::unsupported_capability);auto policy=vita::runtime::transaction::iq_validate({2,static_cast<std::uint8_t>(bit)},*v);assert(!policy.resolvable&&(policy.diagnostics.errors&vita::runtime::transaction::unsupported));};
 for(auto bit:u32bits){check(bit,0);check(bit,0xfedcba98);}for(auto bit:g16bits){check(bit,0);check(bit,65535);for(unsigned r=16;r<32;++r)rejected(packet(bit,1u<<r));}check(31,0);check(31,1);for(unsigned r=1;r<32;++r)rejected(packet(31,1u<<r));
 for(unsigned bit=3;bit<=31;++bit){auto query=words({0x60000008,1,0xa0040000,9,2,3,4,1u<<bit});auto q=decode_packet(query);assert(q&&q->fields.size()==1&&q->fields[0].bytes.empty());auto diag=words({0x64000009,1,0xa90a0400,9,2,3,4,1u<<bit,0x80000000});auto d=decode_packet(diag,DecodeOptions{RequestContext{0xa90a0000}});assert(d&&d->fields.size()==1&&d->fields[0].bytes.size()==4&&*d->fields[0].diagnostic()==0x80000000);}
 // Generic reserved/unregistered CIF3 bit0 remains unsupported after CIF2 expansion.
 rejected(words({0x40000005,1,8,1,0}));
}
