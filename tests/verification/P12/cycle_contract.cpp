#include "fixture.hpp"
using namespace verify_p12;
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;if(!counters.add({7,0x01020304,codec::PacketType::signal})||!counters.add({7,0x01020304,codec::PacketType::command}))return 1;routes.freeze();counters.freeze();auto configured=config();configured.capabilities.reserved_control_slots=20;configured.capabilities.reserved_cancellation_slots=20;auto made=Udp<128,8,8>::create(configured,pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;auto& adapter=**made;auto txpool=verify_p10::external_pool(64,256);CompletionArena<128> tickets;
 for(unsigned n=0;n<65;++n){auto bytes=verify_p12::signal;bytes[1]=std::byte(n%16);auto ticket=tickets.reserve(n+1);if(!ticket||!adapter.try_send({storage(txpool,bytes),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::signal}}))return 3;}
 for(unsigned n=0;n<33;++n){const bool cancel=n%2;std::array<std::byte,20> bytes{};bytes[0]=std::byte(cancel?0x61:0x60);bytes[1]=std::byte(n%16);bytes[3]=std::byte{5};bytes[4]=std::byte{1};bytes[5]=std::byte{2};bytes[6]=std::byte{3};bytes[7]=std::byte{4};bytes[8]=std::byte(cancel?9:1);bytes[9]=std::byte(cancel?8:0);bytes[15]=std::byte{42};auto ticket=tickets.reserve(n+66);if(!ticket||!adapter.try_send({storage(txpool,bytes),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::command}}))return 4;}
 adapter.begin_cycle();for(unsigned n=0;n<200;++n){auto progressed=adapter.progress_next();if(!progressed)return 5;if(!*progressed)break;}
 if(adapter.metrics().tx_data_attempts!=64||adapter.metrics().tx_control_attempts!=32||adapter.outstanding()!=2)return 6;
 // Repeated inner polling must not replenish a host cycle's service allowance.
 for(unsigned n=0;n<10;++n){auto progressed=adapter.progress_next();if(!progressed||*progressed)return 7;}
 adapter.begin_cycle();for(unsigned n=0;n<4&&adapter.outstanding();++n)if(!adapter.progress_next())return 8;
 if(adapter.outstanding()||adapter.metrics().tx_data_attempts!=65||adapter.metrics().tx_control_attempts!=33)return 9;return 0;}
