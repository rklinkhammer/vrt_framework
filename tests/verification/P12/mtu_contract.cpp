#include "fixture.hpp"
using namespace verify_p12;
int main(){for(bool v6:{false,true}){Peer peer(v6);AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;if(!counters.add({7,0x01020304,codec::PacketType::signal}))return 1;routes.freeze();counters.freeze();auto configured=config(v6);for(auto&s:configured.sockets)s.ip_mtu=v6?1280:100;auto made=Udp<8,8,8>::create(configured,pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer,v6)))return 2;auto& adapter=**made;auto& stats=adapter.socket_stats(Lane::data);if(!stats.no_fragment||!stats.nonblocking||stats.send_buffer_bytes<=0||stats.receive_buffer_bytes<=0)return 3;
 const auto maximum=v6?1232u:72u;std::array<std::byte,1240> bytes{};std::copy(verify_p12::signal.begin(),verify_p12::signal.end(),bytes.begin());auto set_length=[&](unsigned length){bytes[2]=std::byte((length/4)>>8);bytes[3]=std::byte(length/4);};auto pool=verify_p10::external_pool(2048,8);CompletionArena<3> tickets;
 set_length(maximum+4);auto ticket=tickets.reserve(1);if(!ticket)return 4;auto rejected=adapter.try_send({storage(pool,Bytes{bytes}.first(maximum+4)),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::signal}});if(rejected||rejected.error().error.code!=ErrorCode::short_output||!rejected.error().submission.completion.is_reserved()||admission.used(Resource::completion))return 5;
 std::array<std::byte,2048> received{};if(peer.receive(received,0)>=0)return 6;
 // Exact-fit packet is accepted with the original packet count: rejection did
 // not consume the sender's counter or send a prefix of the oversized packet.
 set_length(maximum);auto accepted_ticket=tickets.reserve(2);if(!accepted_ticket)return 7;auto accepted=adapter.try_send({storage(pool,Bytes{bytes}.first(maximum)),std::move(*accepted_ticket),{7,1},{7,0x01020304,codec::PacketType::signal}});if(!accepted)return 8;for(unsigned n=0;n<6&&adapter.outstanding(*accepted);++n)if(!adapter.progress_next())return 9;
 if(peer.receive(received)!=maximum)return 10;for(unsigned n=0;n<maximum;++n)if(received[n]!=bytes[n])return 11;
 }return 0;}
