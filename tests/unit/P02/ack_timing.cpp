#include <vita/codec/wire.hpp>
#include <array>
#include <cassert>
using namespace vita;using namespace vita::codec;
int main(){
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.ack=true;e.command=Command{1u<<19,1};
    std::array<std::byte,64> out{};
    for(unsigned mode:{1u,2u,3u,4u,7u}){
        e.command->cam=(1u<<19)|(mode<<12);auto encoded=encode_envelope(e,{},std::nullopt,out);assert(encoded);
        auto parsed=decode_envelope(Bytes{out}.first(*encoded));assert(parsed&&((parsed->envelope.command->cam>>12)&7)==mode&&parsed->envelope.timestamp.tsi==Tsi::none);
    }
    for(unsigned mode:{5u,6u}){e.command->cam=(1u<<19)|(mode<<12);assert(!encode_envelope(e,{},std::nullopt,out));}
    e.ack=false;for(unsigned mode:{1u,2u,3u,4u}){e.command->cam=mode<<12;assert(!encode_envelope(e,{},std::nullopt,out));}
}
