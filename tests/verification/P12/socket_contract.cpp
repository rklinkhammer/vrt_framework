#include "fixture.hpp"
#include <chrono>
#include <thread>
using namespace verify_p12;
int main(){
 for(bool v6:{false,true}){
  Peer peer(v6);if(!peer.valid())return 1;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;Capture capture;capture.retain=true;
  if(!routes.add(route(capture))||!counters.add({7,0x01020304,codec::PacketType::signal}))return 2;routes.freeze();counters.freeze();
  auto made=Udp<8,8,8>::create(config(v6),pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer,v6)))return 3;auto& adapter=**made;
  if(!peer.send_to(verify_p12::signal,v6,adapter.local_address(Lane::data).port))return 4;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);while(!capture.calls&&std::chrono::steady_clock::now()<deadline){auto progress=adapter.progress_next();if(!progress)return 5;std::this_thread::yield();}
  if(capture.calls!=1||capture.bytes!=8||capture.payload[0]!=std::byte{0x40}||!capture.held)return 6;
  auto txpool=verify_p10::external_pool(64,4);CompletionArena<2> tickets;auto ticket=tickets.reserve(1);if(!ticket)return 7;auto submitted=adapter.try_send({storage(txpool,verify_p12::signal),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::signal}});if(!submitted||!adapter.outstanding(*submitted)||tickets.consume(0))return 8;
  for(unsigned n=0;n<6&&adapter.outstanding(*submitted);++n)if(!adapter.progress_next())return 9;
  auto done=tickets.consume(0);if(!done||done->result.status!=CompletionStatus::succeeded||adapter.outstanding(*submitted))return 10;
  // Completion already happened before the receiving application reads bytes.
  std::array<std::byte,64> received{};if(peer.receive(received)!=16)return 11;for(unsigned n=0;n<16;++n)if(received[n]!=verify_p12::signal[n])return 12;if(peer.receive(received,0)>=0)return 13;
  made->reset();auto held=capture.held->fragment(0);if(!held||held->size()!=8||(*held)[0]!=std::byte{0x40})return 14;capture.held.reset();
 }
 return 0;}
