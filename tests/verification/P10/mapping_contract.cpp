#include "runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace verify_p10;
struct Samples{unsigned count=0;std::array<timing::ProtocolTime,8> times{};bool known=true;static void receive(void*p,const BorrowedSignalRx&s)noexcept{auto&c=*static_cast<Samples*>(p);if(c.count==8)std::abort();c.times[c.count++]=s.sample_time;c.known&=s.metadata.confidence==Confidence::known&&s.metadata.valid_data;}};
int main(){using Runtime=VitaRuntime<1,4,32,65536>;
 for(unsigned overlap=0;overlap<3;++overlap){auto instance=Runtime::create(runtime_config(),external_pools());if(!instance)return 1;auto&r=**instance;Samples samples;auto config=stream_config();config.ip_mtu=100;config.receiver={&samples,Samples::receive,nullptr};auto device=r.add_controllee(config);if(!device||!r.observe_pps({0},{1000,0})||!device->start()||!r.progress({0})||samples.count!=1)return 2;
  // First accepted packet samples0..10us. A -0.5us correction leaves next
  // firstsample10.5us after lastsample10us; -2us reuses covered time at9us.
  auto changed=overlap?r.observe_pps({2000},{1000,overlap==2?1000000ULL:0ULL}):r.observe_pps({1000},{1000,500000});if(!changed)return 3;
  if(overlap){if(device->status()!=SourceStatus::temporal_association||device->start())return 4;auto controller=r.add_controller(*device);if(!controller)return 5;auto query=controller->query();if(!query)return 6;for(unsigned i=0;i<10;++i)if(!r.progress({3000+i}))return 7;auto state=controller->state(*query);if(!state||!*state||(**state).state.fields[1].validity!=Validity::known||samples.count!=1)return 8;}
  else {if(device->status()!=SourceStatus::running||!r.progress({11000})||samples.count!=2||samples.times[1]!=timing::ProtocolTime{1000,10500000}||!samples.known)return 9;}
 }
 {
  auto instance=Runtime::create(runtime_config(),external_pools());if(!instance)return 10;auto&r=**instance;Samples samples;auto config=stream_config();config.receiver={&samples,Samples::receive,nullptr};auto device=r.add_controllee(config);if(!device||!r.observe_pps({0},{1000,0})||!device->start()||!r.progress({0}))return 11;
  if(!r.observe_pps({10000000},{2000,0})||!r.progress({10000000}))return 12;
  const auto metrics=device->metrics();if(metrics.skipped_samples>10256||device->timeline().ordinal()>10512||device->status()!=SourceStatus::running)return 13;
  if(!r.progress({10240000})||samples.count<2||!samples.known||samples.times[samples.count-1].seconds<1999)return 14;
 }
 return 0;}
