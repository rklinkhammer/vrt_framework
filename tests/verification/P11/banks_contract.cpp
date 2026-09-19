#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace verify_p10;
int main(){using Runtime=VitaRuntime<1,4,32,65536>;
 {
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 2;
  for(unsigned iteration=0;iteration<6;++iteration){RecoveryConfig recovery{4+iteration,device->confirmed_state(),true};recovery.confirmed_state.fields[0].value=std::uint32_t{4+iteration};if(!device->recover(recovery))return 3;
   for(unsigned n=0;n<8;++n)if(!runtime.progress({iteration*1000+n}))return 4;
   if(device->sid()!=4+iteration||device->lifecycle().phase!=LifecyclePhase::running)return 5;
  }
 }
 {
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 6;auto& runtime=**made;auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 7;auto controller=runtime.add_controller(*device);if(!controller)return 8;
  auto pinned=controller->query();if(!pinned||!runtime.progress({0})||!runtime.progress({1}))return 9;
  RecoveryConfig recovery{4,device->confirmed_state(),true};recovery.confirmed_state.fields[0].value=std::uint32_t{4};if(!device->recover(recovery))return 10;
  for(unsigned n=2;n<10;++n)if(!runtime.progress({n}))return 11;
  recovery.new_sid=5;recovery.confirmed_state.fields[0].value=std::uint32_t{5};
  if(device->recover(recovery)||device->sid()!=4)return 12;
  // Retention time passing does not revoke an application-held transaction.
  if(!runtime.observe_pps({31000000000},{1031,0})||!runtime.progress({31000000000}))return 13;
  if(device->recover(recovery))return 14;
  if(!controller->release(*pinned)||!runtime.progress({31000000001}))return 15;
  if(!device->recover(recovery))return 16;
  for(unsigned n=2;n<10;++n)if(!runtime.progress({31000000000ULL+n}))return 17;
  if(device->sid()!=5||device->lifecycle().phase!=LifecyclePhase::running)return 18;
 }
 return 0;
}
