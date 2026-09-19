#include <vita/codec/packet.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <algorithm>
#include <cassert>
#include <limits>
#include <vector>
using namespace vita;
using namespace vita::codec;
static std::vector<std::byte> bytes(std::initializer_list<std::uint32_t> words){
    std::vector<std::byte> b;for(auto w:words)for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));return b;
}
struct Case{FieldId id;SemanticValue value;std::vector<std::byte> wire;};
int main(){
    static_assert(sizeof(SemanticValue)==16);
    const std::array cases{
        Case{{0,29},Hertz{1048576},bytes({0,0x00100000})},
        Case{{0,28},Hertz{-1048576},bytes({0xffffffff,0xfff00000})},
        Case{{0,27},Hertz{INT64_MIN},bytes({0x80000000,0})},
        Case{{0,26},Hertz{-1},bytes({0xffffffff,0xffffffff})},
        Case{{0,25},Hertz{INT64_MAX},bytes({0x7fffffff,0xffffffff})},
        Case{{0,24},DecibelsQ7{-128},bytes({0x0000ff80})},
        Case{{0,23},GainStages{-128,64},bytes({0x0040ff80})},
        Case{{0,22},std::uint32_t{0xffffffff},bytes({0xffffffff})},
        Case{{0,20},Femtoseconds{INT64_MIN},bytes({0x80000000,0})},
        Case{{0,19},std::uint32_t{0x12345678},bytes({0x12345678})},
        Case{{0,18},CelsiusQ6{-17481},bytes({0x0000bbb7})},
        Case{{0,17},DeviceIdentifierValue{0xffffff,0xabcd},bytes({0x00ffffff,0x0000abcd})},
        Case{{0,10},std::uint32_t{0xfedcba98},bytes({0xfedcba98})}
    };
    std::uint32_t mask=0;std::size_t words=3;
    for(auto& c:cases){mask|=1u<<c.id.bit;words+=c.wire.size()/4;}
    auto literal=bytes({0x40000000u|static_cast<unsigned>(words),0x1234,mask});
    for(auto& c:cases)literal.insert(literal.end(),c.wire.begin(),c.wire.end());
    auto decoded=decode_packet(literal);assert(decoded&&decoded->fields.size()==13);
    ContextPacket builder;
    for(auto it=cases.rbegin();it!=cases.rend();++it)assert(builder.set_value(it->id,it->value));
    for(unsigned i=0;i<13;++i){assert(decoded->fields[i].id==cases[i].id);auto v=decoded->fields[i].value();assert(v&&*v==cases[i].value);}
    Envelope e;e.type=PacketType::context;e.stream_id=0x1234;
    std::array<std::byte,512> output;output.fill(std::byte{0xa5});
    auto n=encode_packet(e,builder.freeze(),output);assert(n&&*n==literal.size()&&std::equal(literal.begin(),literal.end(),output.begin()));
    for(std::size_t size=0;size<literal.size();++size){unsigned calls=0;auto r=decode_and_visit(Bytes{literal}.first(size),{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!r&&calls==0);}
    output.fill(std::byte{0xa5});assert(!encode_packet(e,builder.freeze(),MutableBytes{output}.first(literal.size()-1)));assert(std::all_of(output.begin(),output.end(),[](auto x){return x==std::byte{0xa5};}));
    {auto unknown=bytes({0x40000005,1,0x00000008,0x00000001,0});unsigned calls=0;auto r=decode_and_visit(unknown,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!r&&r.error().code==ErrorCode::unsupported_layout&&calls==0);}
    for(auto bit:{24,18,17}){auto malformed=bytes({0x40000004,1,1u<<bit,0x01000000});if(bit==17){malformed[3]=std::byte{5};auto last=bytes({0});malformed.insert(malformed.end(),last.begin(),last.end());}unsigned calls=0;assert(!decode_and_visit(malformed,{},[&](FieldView)noexcept->Result<void>{++calls;return {};}));assert(calls==0);}
    auto device_reserved=bytes({0x40000005,1,1u<<17,1,0x00010000});assert(!decode_packet(device_reserved));
    for(auto bit:{29,18}){auto invalid=bit==29?bytes({0x40000005,1,1u<<29,0xffffffff,0xffffffff}):bytes({0x40000004,1,1u<<18,0x0000bbb6});auto raw=decode_packet(invalid);assert(raw);auto value=raw->fields[0].value();assert(value&&!validate_value({0,static_cast<std::uint8_t>(bit)},*value));auto before=builder.freeze();assert(!builder.set_value({0,static_cast<std::uint8_t>(bit)},*value,true));assert(builder.freeze().generation()==before.generation());}
    assert(!validate_value(DeviceIdentifier::id,DeviceIdentifierValue{0x1000000,0}));
    // Query and cancellation selectors have no value payload regardless of scalar width.
    auto query=bytes({0x60000007,1,0xa0040000,7,2,3,mask});auto qview=decode_packet(query);assert(qview&&qview->fields.size()==13);
    for(std::size_t i=0;i<qview->fields.size();++i){auto& f=qview->fields[i];assert(f.kind==BodyKind::selectors&&f.bytes.empty()&&!f.value());}
    auto cancel=query;cancel[0]=std::byte{0x61};cancel[8]=std::byte{0xa9};cancel[9]=std::byte{0x08};auto cview=decode_packet(cancel);assert(cview&&cview->fields.size()==13);
    QueryPacket qp;for(auto& c:cases)assert(qp.select(c.id));Envelope qe;qe.type=PacketType::command;qe.stream_id=1;qe.command=Command{0xa0040000,7,Identifier::short_id(2),Identifier::short_id(3)};
    auto qn=encode_packet(qe,qp.freeze(),output);assert(qn&&*qn==query.size()&&std::equal(query.begin(),query.end(),output.begin()));
    // Warning diagnostic extent is one word for every selected field, never its scalar extent.
    auto diag=bytes({0x64000014,1,0xa90a0400,7,2,3,mask});for(unsigned i=0;i<13;++i){auto d=bytes({0x80000000});diag.insert(diag.end(),d.begin(),d.end());}
    auto dv=decode_packet(diag,DecodeOptions{RequestContext{0xa90a0000}});assert(dv&&dv->fields.size()==13);for(std::size_t i=0;i<dv->fields.size();++i){auto& f=dv->fields[i];assert(f.kind==BodyKind::diagnostics&&f.bytes.size()==4&&*f.diagnostic()==0x80000000);}
    for(auto& c:cases){QueryPacket q;assert(q.select(c.id));assert(q.with_attributes(attribute_bit(Attribute::minimum)));assert(!q.with_attributes(1u<<18));}
    for(auto& c:cases){auto v=vita::runtime::transaction::iq_validate(c.id,c.value);assert(!v.resolvable&&(v.diagnostics.errors&vita::runtime::transaction::unsupported));}
    // All 17 recognized fields exceed the unchanged per-packet materialization bound.
    auto excessive=literal; excessive[3]=std::byte{29};
    const auto allmask=mask|(1u<<30)|(1u<<21)|(1u<<16)|(1u<<15);
    for(unsigned i=0;i<4;++i)excessive[8+i]=std::byte((allmask>>(24-i*8))&255);
    auto extra=bytes({0,0,0,0,0,0});excessive.insert(excessive.end(),extra.begin(),extra.end());
    unsigned calls=0;auto limited=decode_and_visit(excessive,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!limited&&calls==0&&(limited.error().code==ErrorCode::resource_limit||limited.error().code==ErrorCode::capacity_exhausted));
    // A qualified Context with a new generic scalar must not enter the fixed IQ cache.
    auto timed=bytes({0x40a00008,1,100,0,0,1u<<29,0,0x00100000});auto p=decode_packet(timed);assert(p);
    vita::runtime::context::ReceiverHistory<8> history(1,Tsi::gps);auto reject=history.receive(*p,1,{0});assert(!reject&&reject.error().code==ErrorCode::unsupported_capability&&history.size()==0);
}
