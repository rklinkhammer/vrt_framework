#include "../P12/fixture.hpp"
#include "../P10/canonical_oracle.hpp"
#include <vita/runtime/context/receiver.hpp>
#include <chrono>
#include <thread>
using namespace verify_p12;
struct Receiver {memory::RetentionQuota waiting{0};context::ContextReceiver<8,4> engine;unsigned delivered=0,contexts=0,before=0;bool failed=false;std::uint64_t ingress=0,entered=0,checksum=0;Receiver():engine(waiting,{this,consume,nullptr},1,codec::Tsi::gps,false,0x01020304){}
 static void pre(void*p,const codec::Envelope&)noexcept{auto&r=*static_cast<Receiver*>(p);r.ingress=transaction::steady_trace_ns(nullptr);++r.before;}
 static void consume(void*p,const context::BorrowedSignalRx& signal)noexcept{auto&r=*static_cast<Receiver*>(p);r.entered=transaction::steady_trace_ns(nullptr);auto data=signal.fragment(0);if(!data||data->size()!=1024||signal.metadata.confidence!=context::Confidence::known||!signal.metadata.valid_data||signal.sample_time!=timing::ProtocolTime{1000,0}){r.failed=true;return;}for(unsigned n=0;n<512;++n){auto value=(unsigned((*data)[2*n])<<8)|unsigned((*data)[2*n+1]);if(value!=verify_p10::iq16[n%32])r.failed=true;r.checksum+=value;}++r.delivered;}
 static void receive(void*p,const codec::PacketView& packet,const memory::RxEnvelope& rx)noexcept{auto&r=*static_cast<Receiver*>(p);const auto& e=packet.envelope.envelope;if(e.type==codec::PacketType::context){if(!r.engine.receive_context(packet,1,{r.ingress}))r.failed=true;++r.contexts;}else if(!r.engine.receive_data(rx,{e.timestamp.integer,e.timestamp.fractional},1,{r.ingress}))r.failed=true;}
};
static void put(MutableBytes b,unsigned offset,std::uint32_t value){for(unsigned n=0;n<4;++n)b[offset+n]=std::byte(value>>(24-8*n));}
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;Receiver receiver;for(auto type:{codec::PacketType::context,codec::PacketType::signal}){Route r;r.key.source={9,1};r.key.stream_id=0x01020304;r.key.type=type;r.context=&receiver;r.before_decode=Receiver::pre;r.receive=Receiver::receive;if(!routes.add(r))return 1;}routes.freeze();counters.freeze();auto rxpools=pools();auto made=Udp<8,8,8>::create(config(),rxpools,admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;
 // Literal GPS/picosecond Context: RP, 1MHz signed Q20 rate, valid Data,
 // canonical IQ16 format. No production encoder participates in this oracle.
 constexpr std::array<std::uint32_t,12> words{0x40a0000c,0x01020304,1000,0,0,0x40218000,0x01020304,0x000000f4,0x24000000,0x40040000,0x200003cf,0};std::array<std::byte,48> ctx;for(unsigned n=0;n<words.size();++n)put(ctx,n*4,words[n]);
 std::array<std::byte,1044> data{};put(data,0,0x10a00105);put(data,4,0x01020304);put(data,8,1000);for(unsigned n=0;n<512;++n){auto value=verify_p10::iq16[n%32];data[20+2*n]=std::byte(value>>8);data[21+2*n]=std::byte(value);}
 auto pump=[&](auto predicate){auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);while(!predicate()&&std::chrono::steady_clock::now()<end){if(!(*made)->progress_next())return false;std::this_thread::yield();}return predicate();};
 if(!peer.send_to(ctx,false,(*made)->local_address(Lane::control).port)||!pump([&]{return receiver.contexts==1;}))return 3;auto begin=transaction::steady_trace_ns(nullptr);if(!peer.send_to(data,false,(*made)->local_address(Lane::data).port)||!pump([&]{return receiver.delivered==1;}))return 4;if(receiver.failed||receiver.ingress<begin||receiver.entered<receiver.ingress||receiver.waiting.active()||receiver.engine.waiting())return 5;
 auto malformed=ctx;put(malformed,20,0x40218002);if(!peer.send_to(malformed,false,(*made)->local_address(Lane::control).port)||!pump([&]{return (*made)->metrics().rx_malformed==1;}))return 6;if(receiver.contexts!=1||receiver.delivered!=1)return 7;
 std::vector<memory::BufferLease> held;for(;;){auto l=rxpools[0].acquire({2048});if(!l)break;held.push_back(std::move(*l));}if(held.size()!=8)return 8;if(!peer.send_to(data,false,(*made)->local_address(Lane::data).port)||!pump([&]{return (*made)->metrics().rx_pool_drop==1;}))return 9;if(receiver.delivered!=1)return 10;held.clear();if(!peer.send_to(data,false,(*made)->local_address(Lane::data).port)||!pump([&]{return receiver.delivered==2;}))return 11;if(receiver.failed||receiver.waiting.active())return 12;
 return 0;
}
