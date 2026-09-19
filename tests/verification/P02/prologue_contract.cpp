#include <vita/codec/prologue.hpp>
#include "vectors.hpp"
#include <algorithm>
using namespace vita;using namespace vita::codec;
int main(){
    Envelope data;data.type=PacketType::signal;data.stream_id=1;
    std::array<std::byte,64> output;output.fill(std::byte{0xa5});
    auto layout=measure_prologue(data,8);if(!layout||layout->packet_bytes!=16||layout->prologue_bytes!=8||layout->payload_offset!=8||layout->trailer_offset)return 1;
    auto encoded=encode_prologue(data,8,output);if(!encoded||*encoded!=8||!std::equal(w5.begin(),w5.begin()+8,output.begin()))return 2;
    if(!std::all_of(output.begin()+8,output.end(),[](std::byte b){return b==std::byte{0xa5};}))return 3;
    output.fill(std::byte{0xa5});auto short_out=encode_prologue(data,8,MutableBytes{output}.first(7));
    if(short_out||short_out.error().required_capacity!=8||output[0]!=std::byte{0xa5})return 4;
    data.packet_count=15;data.stream_id=0x10203040;data.class_id=ClassId{0x12345,0x1122,0x3344,3};
    data.timestamp={Tsi::gps,Tsf::picoseconds,0x01020304,0x100000002};data.trailer=data.nd0=data.spectrum=true;
    const auto expected=wire_bytes(std::array<std::uint32_t,7>{0x1faf000a,0x10203040,0x18012345,0x11223344,0x01020304,1,2});
    layout=measure_prologue(data,8);if(!layout||layout->packet_bytes!=40||layout->prologue_bytes!=28||layout->trailer_offset!=36)return 5;
    encoded=encode_prologue(data,8,output);if(!encoded||*encoded!=28||!std::equal(expected.begin(),expected.end(),output.begin())||output[28]!=std::byte{0xa5})return 6;
    output.fill(std::byte{0xa5});const auto trailer=wire_bytes(std::array<std::uint32_t,1>{0x12345678});
    if(!encode_trailer(0x12345678,output)||!std::equal(trailer.begin(),trailer.end(),output.begin())||output[4]!=std::byte{0xa5})return 7;
    output.fill(std::byte{0xa5});if(encode_trailer(1,MutableBytes{output}.first(3))||output[0]!=std::byte{0xa5})return 8;
    if(encode_prologue(data,1,output)||encode_prologue(data,262140,output)||output[0]!=std::byte{0xa5})return 9;
    for(unsigned type=0;type<8;++type)for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf){
        Envelope e;e.type=static_cast<PacketType>(type);if(type!=0&&type!=2)e.stream_id=1;e.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),2,3};if(type>=6)e.command=Command{};
        auto n=encode_prologue(e,4,output);const auto expected_bytes=(1+(type!=0&&type!=2)+(tsi!=0)+2*(tsf!=0)+(type>=6?2:0))*4;
        if(!n||*n!=static_cast<unsigned>(expected_bytes))return 10;
    }
    return 0;
}
