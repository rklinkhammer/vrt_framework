#include <vita/codec/wire.hpp>
#include "vectors.hpp"
#include <algorithm>
using namespace vita;
using namespace vita::codec;
int main(){
    auto decoded=decode_envelope(w1);
    if(!decoded || decoded->payload_offset!=24 || decoded->payload.size()!=4 || decoded->envelope.command->controller.words[0]!=3)return 1;
    Envelope query;query.type=PacketType::command;query.stream_id=1;
    query.command=Command{0xa0040000,1,Identifier::short_id(2),Identifier::short_id(3)};
    const auto cif=wire_bytes(std::array<std::uint32_t,1>{0x00200000});
    std::array<std::byte,128> output;output.fill(std::byte{0xaa});
    auto encoded=encode_envelope(query,cif,std::nullopt,output);
    if(!encoded || *encoded!=w1.size() || !std::equal(w1.begin(),w1.end(),output.begin()))return 2;
    for(std::size_t i=0;i<w1.size();++i)if(decode_envelope(Bytes{w1}.first(i)))return 3;
    auto bad=w1;bad[0]=std::byte{0x80};if(decode_envelope(bad))return 4;
    bad=w1;bad[0]=std::byte{0x62};if(decode_envelope(bad))return 5;
    output.fill(std::byte{0xaa});auto short_out=encode_envelope(query,cif,std::nullopt,MutableBytes{output}.first(4));
    if(short_out || short_out.error().code!=ErrorCode::short_output || short_out.error().required_capacity!=28 || output[0]!=std::byte{0xaa})return 6;
    auto full=wire_bytes(std::array<std::uint32_t,10>{0x1faf000a,0x10203040,0x18012345,0x11223344,0x01020304,1,2,0x40000000,0x3b21187e,0x12345678});
    auto opts=decode_envelope(full);
    if(!opts || opts->envelope.packet_count!=15 || opts->envelope.class_id->pad_bits!=3 || opts->envelope.class_id->oui!=0x12345 ||
       opts->envelope.timestamp.integer!=0x01020304 || opts->envelope.timestamp.fractional!=0x100000002 || !opts->envelope.trailer ||
       !opts->envelope.spectrum || !opts->envelope.nd0 || opts->trailer!=0x12345678)return 7;
    auto reencoded=encode_envelope(opts->envelope,opts->payload,opts->trailer,output);
    if(!reencoded || *reencoded!=full.size() || !std::equal(full.begin(),full.end(),output.begin()))return 8;
    for(unsigned type=0;type<8;++type)for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf){
        Envelope e;e.type=static_cast<PacketType>(type);if(type!=0&&type!=2)e.stream_id=8;
        e.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),3,4};if(type>=6)e.command=Command{};
        const auto body=wire_bytes(std::array<std::uint32_t,1>{0});
        auto n=encode_envelope(e,body,std::nullopt,output);if(!n)return 9;
        const auto expected=(1+(type!=0&&type!=2)+(tsi!=0)+2*(tsf!=0)+(type>=6?2:0)+1)*4;
        auto parsed=decode_envelope(Bytes{output}.first(*n));if(!parsed || *n!=static_cast<unsigned>(expected) || parsed->envelope.type!=e.type)return 10;
    }
    auto uuids=wire_bytes(std::array<std::uint32_t,13>{0x6000000d,1,0xf0040000,7,1,2,3,4,5,6,7,8,0x00200000});
    auto ids=decode_envelope(uuids);if(!ids || ids->envelope.command->controllee.kind!=IdentifierKind::uuid || ids->envelope.command->controller.words[3]!=8 || ids->payload_offset!=48)return 11;
    auto reserved=full;reserved[8]|=std::byte{0x04};if(decode_envelope(reserved))return 12;
    auto trailing=w1;trailing[3]=std::byte{0};if(decode_envelope(trailing))return 13;
    for(unsigned timing=0;timing<8;++timing){
        auto ack=wire_bytes(std::array<std::uint32_t,6>{0x64000006,1,0xa9080400u|(timing<<12),42,2,3});
        if(bool(decode_envelope(ack))!=(timing<5||timing==7))return 14;
        auto control=wire_bytes(std::array<std::uint32_t,6>{0x60000006,1,0xa9000000u|(timing<<12),42,2,3});
        if(bool(decode_envelope(control))!=(timing==0))return 15;
    }
    return 0;
}
