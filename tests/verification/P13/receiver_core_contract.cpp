#include "../../../bench/receiver_core.hpp"
#include "../P12/peer.hpp"
#include "../P10/runtime_fixture.hpp"
#include "../P10/canonical_oracle.hpp"
#include <thread>
using namespace vita;using namespace vita::bench::receiver;using namespace vita::adapters::posix_udp;
static void put(MutableBytes b,unsigned at,std::uint32_t value){for(unsigned n=0;n<4;++n)b[at+n]=std::byte(value>>(24-8*n));}
struct Rows{std::array<Trace,128> rows{};unsigned size=0;static void emit(void*p,const Trace&t)noexcept{auto&s=*static_cast<Rows*>(p);if(s.size==s.rows.size())std::abort();s.rows[s.size++]=t;}};
static std::array<std::byte,56> context_wire(){constexpr std::array<std::uint32_t,14> words{0x48a0000e,1,0xabcdef,0x00010010,1000,0,0,0x40218000,1,0xf4,0x24000000,0x40040000,0x200003cf,0};std::array<std::byte,56>b;for(unsigned n=0;n<14;++n)put(b,n*4,words[n]);return b;}
static std::array<std::byte,1052> data_wire(unsigned ordinal=0){std::array<std::byte,1052>b{};put(b,0,0x18a00107);put(b,4,1);put(b,8,0xabcdef);put(b,12,0x00010001);put(b,16,1000);std::uint64_t ps=ordinal*1000000ull;put(b,20,ps>>32);put(b,24,ps);for(unsigned n=0;n<512;++n){auto v=verify_p10::iq16[n%32];b[28+2*n]=std::byte(v>>8);b[29+2*n]=std::byte(v);}return b;}
int main(){verify_p12::Peer peer;vita::bench::receiver::Config config;config.streams=1;config.warmup_ns=0;config.duration_ns=1000000000;for(unsigned n=0;n<3;++n){config.local[n]=Address::loopback(Family::ipv4,0);config.sender[n]=Address::loopback(Family::ipv4,peer.port());}Rows rows;auto pools=verify_p10::external_pools();pools.rx_data=verify_p10::external_pool(2048,128);auto made=Receiver::create(config,pools,&rows,Rows::emit);if(!made)return 1;auto& r=**made;auto now=[](){return runtime::transaction::steady_trace_ns(nullptr);};auto pump=[&](auto predicate){auto until=now()+100000000;while(!predicate()&&now()<until){r.progress(now());std::this_thread::yield();}return predicate();};auto data=data_wire();auto ctx=context_wire();
 if(!peer.send_to(data,false,r.adapter().local_address(Lane::data).port)||!pump([&]{return r.stats(0).checked==1;}))return 2;const auto original=r.first_rx();if(rows.size||!original)return 3;std::this_thread::sleep_for(std::chrono::milliseconds(1));const auto before_context=now();if(!peer.send_to(ctx,false,r.adapter().local_address(Lane::control).port)||!pump([&]{return rows.size==1;}))return 4;
 const auto& t=rows.rows[0];std::uint64_t expected=14695981039346656037ull;for(unsigned n=28;n<data.size();++n)expected=(expected^unsigned(data[n]))*1099511628211ull;if(t.sid!=1||t.ordinal||t.rx_ns!=original||t.validated_ns<t.rx_ns||t.validated_ns>=before_context||t.app_ns<before_context||t.consumed_ns<t.app_ns||t.status||!t.known||t.bytes!=1024||t.checksum!=expected||t.phase!=1)return 5;
 auto bad=data;bad[30]^=std::byte{1};if(!peer.send_to(bad,false,r.adapter().local_address(Lane::data).port)||!pump([&]{return rows.size==2;}))return 6;if(rows.rows[1].status!=2||r.stats(0).invalid!=1||r.stats(0).delivered!=1)return 7;
 // A separate receiver with no Context holds exactly64 payloads; the next
 // checked packet is an explicit pending overflow, then all64 expire at10ms.
 Rows stalled_rows;auto stalled=Receiver::create(config,pools,&stalled_rows,Rows::emit);if(!stalled)return 8;auto& s=**stalled;
 // Capacity and expiry are separate contracts. Freeze the expiry clock while
 // filling the queue so sanitizer/host throughput cannot expire earlier entries.
 const auto held_clock=now();for(unsigned n=0;n<65;++n){auto b=data_wire(n*256);if(!peer.send_to(b,false,s.adapter().local_address(Lane::data).port))return 9;auto until=now()+100000000;while(s.stats(0).checked<n+1&&now()<until)s.progress(held_clock);if(s.stats(0).checked!=n+1)return 10;}
 if(s.stats(0).pending_overflow!=1||stalled_rows.size!=1||stalled_rows.rows[0].status!=3)return 11;s.progress(now()+10000000);if(stalled_rows.size!=65||s.stats(0).metadata_drops!=64)return 12;for(unsigned n=1;n<65;++n)if(stalled_rows.rows[n].status!=1||stalled_rows.rows[n].app_ns||stalled_rows.rows[n].consumed_ns)return 13;
 return 0;}
