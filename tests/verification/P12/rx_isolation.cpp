#include "fixture.hpp"
#include <chrono>
#include <thread>
using namespace verify_p12;
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;Capture data,control;data.retain=true;if(!routes.add(route(data))||!routes.add(route(control,codec::PacketType::command)))return 1;routes.freeze();counters.freeze();auto receive_pools=pools();receive_pools[0]=verify_p10::external_pool(2048,1);auto made=Udp<8,8,8>::create(config(),receive_pools,admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;auto& adapter=**made;
 auto pump=[&](auto done){auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);while(!done()&&std::chrono::steady_clock::now()<deadline){if(!adapter.progress_next())return false;std::this_thread::yield();}return done();};
 if(!peer.send_to(verify_p12::signal,false,adapter.local_address(Lane::data).port)||!pump([&]{return data.calls==1;})||!data.held)return 3;
 for(unsigned n=0;n<16;++n)if(!peer.send_to(verify_p12::signal,false,adapter.local_address(Lane::data).port))return 4;
 std::array<std::byte,20> command{};command[0]=std::byte{0x60};command[3]=std::byte{5};command[4]=std::byte{1};command[5]=std::byte{2};command[6]=std::byte{3};command[7]=std::byte{4};command[8]=std::byte{1};command[15]=std::byte{42};
 if(!peer.send_to(command,false,adapter.local_address(Lane::control).port)||!pump([&]{return control.calls==1&&adapter.metrics().rx_pool_drop==16;}))return 5;
 if(data.calls!=1||admission.used(Resource::ordinary_queue)||admission.used(Resource::cancellation_queue))return 6;
 return 0;}
