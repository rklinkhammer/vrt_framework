#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace verify_p10;
struct Done{unsigned calls=0;LifecycleStatus status{};static void receive(void*p,const LifecycleStatus&s)noexcept{auto& d=*static_cast<Done*>(p);++d.calls;d.status=s;}};
int main(){using Runtime=VitaRuntime<2,4,32,65536>;
 for(bool immediate:{false,true}){
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;auto device=runtime.add_controllee(stream_config());auto other_config=stream_config();other_config.sid=2;auto other=runtime.add_controllee(other_config);if(!device||!other||!runtime.observe_pps({0},{1000,0}))return 2;
  adapters::loopback::Fault held;held.hold_quiescence=true;held.fail_completion=true;runtime.inject_next_transport_fault(held);
  if(!device->start()||!runtime.progress({0})||!other->start())return 3;
  Done done;if(!device->shutdown(immediate?StopMode::immediate:StopMode::graceful,{&done,Done::receive}))return 4;
  if(!runtime.progress({1})||!device->lifecycle().io)return 5;
  if(immediate){if(done.calls!=1||done.status.phase!=LifecyclePhase::quarantined)return 6;}
  else {
   if(done.calls||!runtime.progress({1999999999})||done.calls)return 7;
   if(!runtime.progress({2000000000})||done.calls!=1||done.status.phase!=LifecyclePhase::quarantined||!done.status.io)return 8;
  }
  // Quarantine is physical ownership, not a synthetic forced buffer return.
  if(!device->lifecycle().io||!runtime.prove_transport_quiescent(*device))return 9;
  if(!runtime.progress({immediate?2ULL:2000000001ULL})||device->lifecycle().io||done.calls!=1)return 10;
  if(!other->metrics().packets)return 11;
 }
 return 0;
}
