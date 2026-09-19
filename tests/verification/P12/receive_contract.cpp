#include "fixture.hpp"
#include <chrono>
#include <thread>
using namespace verify_p12;
int main(){Peer peer,unauthorized;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;Capture capture;if(!routes.add(route(capture)))return 1;routes.freeze();counters.freeze();auto made=Udp<8,8,8>::create(config(),pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;auto& adapter=**made;
 auto sum=[&]{auto&m=adapter.metrics();return m.rx_malformed+m.rx_truncated+m.rx_unauthorized+m.rx_pool_drop+m.rx_lane_drop+m.rx_mtu_drop;};
 auto send=[&](Bytes bytes,Peer& source,Lane lane){const auto before=sum();if(!source.send_to(bytes,false,adapter.local_address(lane).port))return false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);while(sum()==before&&std::chrono::steady_clock::now()<deadline){auto p=adapter.progress_next();if(!p)return false;std::this_thread::yield();}return sum()==before+1&&capture.calls==0;};
 if(!send(verify_p12::signal,unauthorized,Lane::data)||adapter.metrics().rx_unauthorized!=1)return 3;
 if(!send(verify_p12::signal,peer,Lane::control)||adapter.metrics().rx_lane_drop!=1)return 4;
 if(!send({},peer,Lane::data)||!send(Bytes{verify_p12::signal}.first(3),peer,Lane::data))return 5;
 auto zero=verify_p12::signal;zero[3]=std::byte{0};if(!send(zero,peer,Lane::data))return 6;
 if(!send(Bytes{verify_p12::signal}.first(12),peer,Lane::data))return 7;
 std::array<std::byte,20> long_packet{};std::copy(verify_p12::signal.begin(),verify_p12::signal.end(),long_packet.begin());if(!send(long_packet,peer,Lane::data)||adapter.metrics().rx_truncated!=1)return 8;
 auto mtu=verify_p12::signal;mtu[2]=std::byte{0x01};mtu[3]=std::byte{0x80};if(!send(mtu,peer,Lane::data)||adapter.metrics().rx_mtu_drop!=1)return 9;
 if(adapter.metrics().rx_malformed!=4||adapter.metrics().rx_delivered)return 10;return 0;}
