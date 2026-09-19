#include "../../../bench/peer_capture.hpp"
using namespace vita;using namespace vita::bench;
int main(){using namespace adapters::posix_udp;PeerAckPolicy policy{Address::loopback(Family::ipv4,12000),0x00a1b2};codec::Envelope e;e.type=codec::PacketType::command;e.ack=true;e.stream_id=101;e.class_id=codec::ClassId{0x00a1b2,1,0x20,0};e.command=codec::Command{0xa9080400,1,codec::Identifier::short_id(3),codec::Identifier::short_id(2)};
 std::array<std::byte,256> bytes;auto size=codec::encode_envelope(e,{},std::nullopt,bytes);if(!size)return 1;auto packet=codec::decode_packet(Bytes{bytes}.first(*size),{codec::RequestContext{0xa91f0000}});if(!packet)return 2;
 // The oldest issued MID remains capturable after >512 subsequent sends. No
 // recording a newer Ack is allowed to evict its meaning or alter its identity.
 for(unsigned next:{2u,514u,100000u}){auto late=capture_ack(policy,policy.expected_source,false,*packet,next,123456);if(!late||late->mid!=1||late->sid!=101||late->kind!=3||!late->success||late->monotonic_ns!=123456)return 3;}
 auto duplicate=capture_ack(policy,policy.expected_source,false,*packet,100000,123457);if(!duplicate||duplicate->mid!=1)return 4;
 if(capture_ack(policy,policy.expected_source,true,*packet,100000,1)||capture_ack(policy,Address::loopback(Family::ipv4,12001),false,*packet,100000,1)||capture_ack(policy,policy.expected_source,false,*packet,1,1))return 5;
 auto bad=*packet;bad.envelope.envelope.stream_id=106;if(capture_ack(policy,policy.expected_source,false,bad,100000,1))return 6;bad=*packet;bad.envelope.envelope.class_id->oui=0x123456;if(capture_ack(policy,policy.expected_source,false,bad,100000,1))return 7;bad=*packet;bad.envelope.envelope.command->controller=codec::Identifier::short_id(99);if(capture_ack(policy,policy.expected_source,false,bad,100000,1))return 8;bad=*packet;bad.envelope.envelope.cancel=true;if(capture_ack(policy,policy.expected_source,false,bad,100000,1))return 9;
 return 0;
}
