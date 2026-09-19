#include "fixture.hpp"
using namespace verify_p12;
static std::array<std::byte,20> command(bool cancel,unsigned count){std::array<std::byte,20>b{};b[0]=std::byte(cancel?0x61:0x60);b[1]=std::byte(count);b[3]=std::byte{5};b[4]=std::byte{1};b[5]=std::byte{2};b[6]=std::byte{3};b[7]=std::byte{4};b[8]=std::byte(cancel?9:1);if(cancel)b[9]=std::byte{0x08};b[15]=std::byte{42};return b;}
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;if(!counters.add({7,0x01020304,codec::PacketType::signal})||!counters.add({7,0x01020304,codec::PacketType::command}))return 1;routes.freeze();counters.freeze();auto made=Udp<8,8,8>::create(config(),pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;auto& adapter=**made;auto txpool=verify_p10::external_pool(64,32);CompletionArena<10> tickets;
 for(unsigned i=0;i<6;++i){auto bytes=verify_p12::signal;bytes[1]=std::byte(i);auto ticket=tickets.reserve(i+1);if(!ticket||!adapter.try_send({storage(txpool,bytes),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::signal}}))return 3;}
 auto bytes=verify_p12::signal;bytes[1]=std::byte{6};auto rejected_ticket=tickets.reserve(7);if(!rejected_ticket)return 4;auto rejected=adapter.try_send({storage(txpool,bytes),std::move(*rejected_ticket),{7,1},{7,0x01020304,codec::PacketType::signal}});if(rejected||!rejected.error().submission.completion.is_reserved()||rejected.error().submission.storage.byte_size()!=16)return 5;
 if(admission.used(Resource::ordinary_queue)||admission.used(Resource::cancellation_queue)||admission.used(Resource::data_queue)!=6)return 6;
 for(bool cancel:{false,true}){auto ticket=tickets.reserve(cancel?9:8);auto bytes=command(cancel,cancel?1:0);if(!ticket||!adapter.try_send({storage(txpool,bytes),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::command}}))return 7;}
 if(adapter.outstanding()!=8)return 8;for(unsigned n=0;n<3;++n)if(!adapter.progress_next())return 9;
 std::array<std::byte,64> received{};unsigned data=0,control=0,cancel=0;for(unsigned n=0;n<3;++n){auto bytes=peer.receive(received);if(bytes<4)return 10;auto type=std::uint8_t(received[0]);data+=type==0x10;control+=type==0x60;cancel+=type==0x61;}
 if(data!=1||control!=1||cancel!=1||admission.used(Resource::ordinary_queue)||admission.used(Resource::cancellation_queue))return 11;
 adapter.abort();for(unsigned n=0;n<8&&adapter.outstanding();++n)if(!adapter.progress_next())return 12;
 if(adapter.outstanding()||adapter.metrics().tx_failed!=5||admission.used(Resource::data_queue))return 13;
 return 0;}
