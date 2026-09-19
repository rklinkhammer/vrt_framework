#include <vita/codec/packet.hpp>
#include <algorithm>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static std::vector<std::byte> words(std::initializer_list<std::uint32_t> input){std::vector<std::byte> out;for(auto w:input)for(int i=3;i>=0;--i)out.push_back(std::byte((w>>(8*i))&255));return out;}
static void put(std::vector<std::byte>& out,std::size_t n,std::uint32_t w){for(unsigned i=0;i<4;++i)out[n*4+i]=std::byte((w>>(24-i*8))&255);}
static std::vector<std::byte> packet(unsigned bit,const std::vector<std::byte>& field){auto out=words({0x40000000u|static_cast<std::uint32_t>(3+field.size()/4),1,1u<<bit});out.insert(out.end(),field.begin(),field.end());return out;}
static void barrier(const std::vector<std::byte>& wire){unsigned calls=0;assert(!decode_and_visit(wire,{},[&](FieldView)noexcept->Result<void>{++calls;return {};}));assert(calls==0);}
int main(){
    // Embedded GPS2/picoseconds2 differs from untimestamped enclosing Context.
    const auto geo=words({0x0affffff,123,0,456,0x16800000,0xd3000000,0xffffffe0,0x00018000,0x3c000000,0x7fffffff,0xffc00000});
    const auto eph=words({0x0d123456,999,0x12345678,0xabcdef01,0xffffffe0,0x00000020,0x7fffffff,0xffc00000,0x00800000,0x00000001,0xffff0000,0x00018000,0x7fffffff});
    for(unsigned bit:{14u,13u,12u,11u}){const auto& body=bit>=13?geo:eph;auto wire=packet(bit,body);auto p=decode_packet(wire);assert(p&&p->fields.size()==1&&p->fields[0].id==(FieldId{0,static_cast<std::uint8_t>(bit)})&&p->fields[0].bytes.size()==body.size());assert(std::equal(body.begin(),body.end(),p->fields[0].bytes.begin()));
        if(bit>=13){auto g=p->fields[0].geolocation();assert(g&&g->fix.tsi==2&&g->fix.tsf==2&&g->fix.seconds==123&&g->fix.fractional==456&&g->latitude_q22==90*(1<<22)&&g->longitude_q22==-180*(1<<22)&&g->speed_q16==98304&&!g->track_q22);}
        else{auto e=p->fields[0].ephemeris();assert(e&&e->fix.tsi==3&&e->fix.tsf==1&&e->fix.fractional==0x12345678abcdef01ull&&e->position_q5[0]==-32&&!e->position_q5[2]&&e->velocity_q16[0]==-65536);}
NativeContextPacket<> native;assert(p->fields[0].materialize_into(native));std::array<std::byte,128> encoded{};Envelope env;env.type=PacketType::context;env.stream_id=1;auto len=encode_packet(env,native.freeze(),encoded);assert(len&&*len==wire.size()&&std::equal(wire.begin(),wire.end(),encoded.begin()));
        for(std::size_t size=0;size<wire.size();++size)barrier({wire.begin(),wire.begin()+size});auto reserved=wire;reserved[12]|=std::byte{0x80};barrier(reserved);
        auto unknown=body;put(unknown,0,0x00ffffff);put(unknown,1,0xffffffff);put(unknown,2,0xffffffff);put(unknown,3,0xffffffff);for(std::size_t n=4;n<unknown.size()/4;++n)put(unknown,n,0x7fffffff);assert(decode_packet(packet(bit,unknown)));put(unknown,1,0);barrier(packet(bit,unknown));
    }
    for(auto pair:std::array<std::pair<unsigned,std::uint32_t>,3>{{{4,91u*(1u<<22)},{7,0xffffffffu},{8,360u*(1u<<22)}}}){auto invalid=geo;put(invalid,pair.first,pair.second);auto wire=packet(14,invalid);auto decoded=decode_packet(wire);assert(decoded&&decoded->fields[0].geolocation());NativeContextPacket<> dest;assert(!decoded->fields[0].materialize_into(dest));assert(dest.freeze().fields().empty());}
    // Materialization must not silently relabel a scalar Minimum attribute as Current.
    auto scalar_bytes=words({0,0x00100000});FieldView minimum{SampleRate::id,Attribute::minimum,BodyKind::values,DiagnosticGroup::none,scalar_bytes};NativeContextPacket<> target;assert(!minimum.materialize_into(target));assert(target.freeze().fields().empty());
    // Relative must consume13words; the anomalous11word diagram must not shift following fields.
    auto short_relative=eph;short_relative.resize(44);barrier(packet(11,short_relative));
    const auto ascii=words({0x00123456,2,0x24412c31,0x0d0a0000});auto a=packet(9,ascii);auto ap=decode_packet(a);assert(ap&&ap->fields[0].bytes.size()==16);
    auto nonascii=a;nonascii[20]=std::byte{0x80};barrier(nonascii);auto interior=a;interior[20]=std::byte{0};barrier(interior);auto count=a;put(count,4,0xffffffff);barrier(count);
    assert(decode_packet(packet(9,words({0x00ffffff,0}))));
    const auto lists=words({0x00020001,0x00018002,1,1,2,3,4,5,0xffffffff,7});auto l=packet(8,lists);auto lp=decode_packet(l);assert(lp&&lp->fields[0].bytes.size()==40);
    // Duplicates and zero SIDs are representation, not automatic graph inference.
    assert(decode_packet(packet(8,words({0x00010000,0,0}))));assert(decode_packet(packet(8,words({0,0x00008000}))));
    for(auto mask:{0x80000000u,0x02000000u,0x00008000u,0x00000200u}){auto bad=l;put(bad,3,0x00020001|mask);barrier(bad);}
    for(std::size_t size=12;size<l.size();size+=4){auto short_field=l;short_field.resize(size);put(short_field,0,0x40000000u|static_cast<std::uint32_t>(size/4));barrier(short_field);}
    // Early valid fixed field cannot trigger callbacks before a later variable-tail failure.
    auto both=words({0x40000012,1,(1u<<14)|(1u<<8)});both.insert(both.end(),geo.begin(),geo.end());auto badlists=words({1,0,2});both.insert(both.end(),badlists.begin(),badlists.end());put(both,0,0x40000000u|static_cast<std::uint32_t>(both.size()/4));put(both,14,2);barrier(both);
}
