#include "../P12/fixture.hpp"
#include <chrono>
#include <thread>
using namespace verify_p12;
struct Ingress {unsigned before=0,delivered=0;std::uint64_t timestamp=0;static void capture(void*p,const codec::Envelope&e)noexcept{auto&i=*static_cast<Ingress*>(p);if(!e.command||e.command->message_id!=42)std::abort();++i.before;i.timestamp=transaction::steady_trace_ns(nullptr);}static void receive(void*p,const codec::PacketView&,const memory::RxEnvelope&)noexcept{++static_cast<Ingress*>(p)->delivered;}};
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;Ingress ingress;Route route;route.key.source={9,1};route.key.stream_id=0x01020304;route.key.type=codec::PacketType::command;route.context=&ingress;route.before_decode=Ingress::capture;route.receive=Ingress::receive;if(!routes.add(route))return 1;routes.freeze();counters.freeze();auto made=Udp<8,8,8>::create(config(),pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;
 // Valid framing/identity, malformed semantic body: CIF1 presence without CIF1.
 std::array<std::byte,20> packet{};packet[0]=std::byte{0x60};packet[3]=std::byte{5};packet[4]=std::byte{1};packet[5]=std::byte{2};packet[6]=std::byte{3};packet[7]=std::byte{4};packet[8]=std::byte{1};packet[15]=std::byte{42};packet[19]=std::byte{2};
 const auto before=transaction::steady_trace_ns(nullptr);if(!peer.send_to(packet,false,(*made)->local_address(Lane::control).port))return 3;auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);while(!(*made)->metrics().rx_malformed&&std::chrono::steady_clock::now()<deadline){if(!(*made)->progress_next())return 4;std::this_thread::yield();}
 if(ingress.before!=1||ingress.delivered||ingress.timestamp<before||(*made)->metrics().rx_malformed!=1)return 5;return 0;}
