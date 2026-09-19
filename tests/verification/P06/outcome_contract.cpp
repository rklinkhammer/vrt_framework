#include <vita/runtime/transaction/outcomes.hpp>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
static std::uint32_t word(Bytes b,std::size_t offset){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v=(v<<8)|std::to_integer<unsigned>(b[offset+i]);return v;}
int main(){
    AckRecord ack;ack.request.type=PacketType::command;ack.request.stream_id=1;ack.request.command=Command{0xa9080000,42,Identifier::short_id(2),Identifier::short_id(3)};ack.cam.raw=0xa9080000;ack.cam.action=2;ack.kind=AckKind::execution;ack.scheduled_or_executed=true;ack.time_known=true;ack.time={123,456};
    std::array<std::byte,128> output;
    if(encode_response(ack,output))return 1;
    for(unsigned epoch=1;epoch<4;++epoch){ack.epoch=static_cast<Tsi>(epoch);auto n=encode_response(ack,output);if(!n||*n!=36)return 2;
        if(word(output,0)!=(0x64200009u|(epoch<<22))||word(output,8)!=123||word(output,12)!=0||word(output,16)!=456||word(output,20)!=0xa9080400)return 3;
    }
    ack.time.picoseconds=1000000000000ull;if(encode_response(ack,output))return 4;
    ack.time={UINT64_C(0x100000000),0};if(encode_response(ack,output))return 5;
    ack.time_known=false;ack.timing=7;ack.partial=true;ack.scheduled_or_executed=false;
    ack.request.timestamp={Tsi::gps,Tsf::picoseconds,100,0};auto failure=encode_response(ack,output);if(!failure||*failure!=24||word(output,8)!=0xa9087800)return 6;
    ack.request.timestamp={};auto untimed=encode_response(ack,output);if(!untimed||(word(output,8)&0x7000))return 7;
    ack.request.packet_count=5;auto count=encode_response(ack,output,13);if(!count||((word(output,0)>>16)&15)!=13)return 8;
    if(encode_response(ack,output,16))return 9;
    auto default_count=encode_response(ack,output);if(!default_count||((word(output,0)>>16)&15)!=0)return 10;
    return 0;
}
