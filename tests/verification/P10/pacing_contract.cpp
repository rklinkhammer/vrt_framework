#include "runtime_fixture.hpp"
#include "canonical_oracle.hpp"
#include <cstdio>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace verify_p10;
struct Capture{unsigned count=0;std::array<timing::ProtocolTime,16> times;std::array<std::uint32_t,16> first;std::array<std::size_t,16> bytes{};std::array<MetadataSnapshot,16> metadata;
 static void receive(void*p,const BorrowedSignalRx& signal)noexcept{auto&c=*static_cast<Capture*>(p);if(c.count==16)std::abort();auto i=c.count++;c.times[i]=signal.sample_time;c.metadata[i]=signal.metadata;auto fragment=signal.fragment(0);if(!fragment||fragment->size()<4)std::abort();for(auto b:*fragment){(void)b;++c.bytes[i];}c.first[i]=(std::uint32_t((*fragment)[0])<<24)|(std::uint32_t((*fragment)[1])<<16)|(std::uint32_t((*fragment)[2])<<8)|std::uint32_t((*fragment)[3]);}};
int main(){using Runtime=VitaRuntime<2,4,32,65536>;auto instance=Runtime::create(runtime_config(),external_pools());if(!instance){std::fprintf(stderr,"create %d\n",int(instance.error().code));return 1;}auto& r=**instance;Capture capture;auto config=stream_config();config.ip_mtu=100;config.receiver={&capture,Capture::receive,nullptr};auto device=r.add_controllee(config);if(!device)return 2;if(!r.observe_pps({0},{1000,0})||!device->start()||!r.progress({0}))return 3;
 if(capture.count!=1||capture.bytes[0]!=44||capture.times[0]!=timing::ProtocolTime{1000,0}||capture.first[0]!=0x40000000||capture.metadata[0].confidence!=Confidence::known)return 4;
 if(!r.progress({0})||capture.count!=1)return 5;
 if(!r.progress({1000000})||capture.count!=2||capture.times[1]!=timing::ProtocolTime{1000,990000000}||capture.first[1]!=(std::uint32_t(verify_p10::iq16[28])<<16|verify_p10::iq16[29]))return 6;
 auto metrics=device->metrics();if(metrics.samples!=22||metrics.skipped_packets!=89||metrics.skipped_samples!=979||device->timeline().ordinal()!=1001)return 7;
 if(!r.progress({1000000})||capture.count!=2)return 8;
 if(!device->stop()||!r.progress({2000000})||capture.count!=2||!device->start()||!r.progress({2000000}))return 9;
 if(capture.count!=2||device->timeline().ordinal()!=2002||!r.progress({2002000}))return 14;
 if(capture.count!=3||device->timeline().ordinal()!=2013||capture.metadata[2].confidence!=Confidence::known||capture.times[2]!=timing::ProtocolTime{1000,2002000000}||device->metrics().skipped_samples!=1980)return 10;
 {
  auto stalled=Runtime::create(runtime_config(),external_pools());if(!stalled)return 11;Capture seen;auto c=stream_config();c.receiver={&seen,Capture::receive,nullptr};auto d=(*stalled)->add_controllee(c);if(!d||!(*stalled)->observe_pps({0},{1000,0})||!d->start()||!(*stalled)->progress({0})||!(*stalled)->progress({1000000000}))return 12;
  if(d->status()!=SourceStatus::running||seen.count!=2||seen.metadata[1].confidence!=Confidence::known||d->timeline().ordinal()>1000512||d->metrics().skipped_samples>1000256)return 13;
 }
 return 0;
}
