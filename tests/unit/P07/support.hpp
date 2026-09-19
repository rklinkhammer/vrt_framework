#pragma once
#include <vita/runtime/transaction/manager.hpp>
#include <cassert>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;
struct Wire {
    std::array<std::byte,256> bytes{};std::size_t size=0;
    codec::PacketView view(){auto v=codec::decode_packet(Bytes{bytes}.first(size));assert(v);return *v;}
};
Wire make(bool cancel=false,bool multi=false,std::uint32_t rate=2,std::uint32_t mid=7) {
    Wire wire;codec::Envelope e;e.type=codec::PacketType::command;e.stream_id=1;e.cancel=cancel;
    e.command=codec::Command{cancel?0x090d0000u:0x091f0000u,mid};
    Result<std::size_t> n;
    if(cancel){CancelPacket p;assert(p.select(SampleRate::id));if(multi)assert(p.select(ReferencePoint::id));n=codec::encode_packet(e,p.freeze(),wire.bytes);}
    else{ControlPacket p;assert(p.set<SampleRate>(*Hertz::from_integer(rate)));if(multi)assert(p.set<ReferencePoint>(9));n=codec::encode_packet(e,p.freeze(),wire.bytes);}
    assert(n);wire.size=*n;return wire;
}
StateSnapshot initial(){StateSnapshot s;for(auto& f:s.fields)f.validity=Validity::known;s.fields[0].value=std::uint32_t{1};s.fields[1].value=*Hertz::from_integer(1);s.fields[2].value=std::uint32_t{0};s.fields[3].value=PayloadFormat{0x200003cf00000000ull};return s;}
