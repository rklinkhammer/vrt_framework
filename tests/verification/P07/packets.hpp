#pragma once
#include <vita/codec/packet.hpp>
#include <array>
#include <cstdlib>
namespace verify_p07 {
using namespace vita;using namespace vita::codec;
struct Packet {
    std::array<std::byte,512> bytes{};std::size_t size=0;
    PacketView view() const {auto p=decode_packet(Bytes{bytes}.first(size));if(!p)std::abort();return *p;}
};
inline Packet make(bool cancel,std::uint32_t mid=42,unsigned fields=3,std::int64_t rate=2,std::uint32_t cam=0xa91f0000,unsigned count=0){
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.packet_count=count;e.cancel=cancel;e.command=Command{cam,mid,Identifier::short_id(2),Identifier::short_id(3)};
    Packet result;Result<std::size_t> size;
    if(cancel){CancelPacket p;if(fields&1)p.select<ReferencePoint>();if(fields&2)p.select<SampleRate>();if(fields&4)p.select<StateEvent>();size=encode_packet(e,p.freeze(),result.bytes);}
    else{ControlPacket p;p.configure(0,(cam>>23)&3);if(fields&1)p.set<ReferencePoint>(7);if(fields&2)p.set<SampleRate>(*Hertz::from_integer(rate));if(fields&4)p.set<StateEvent>(0);size=encode_packet(e,p.freeze(),result.bytes);}
    if(!size)std::abort();result.size=*size;return result;
}
inline Packet cancellation(std::uint32_t mid=42,unsigned fields=3,std::uint32_t cam=0xa90f0000,unsigned count=0){return make(true,mid,fields,2,cam,count);}
} // namespace verify_p07
