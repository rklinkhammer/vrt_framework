#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p10;
int main(){using Runtime=VitaRuntime<1,4,32,65536>;auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 2;auto controller=runtime.add_controller(*device);if(!controller||!runtime.configure_virtual_backend(*device,SampleRate::id,{},false))return 3;
 const auto confirmed=device->confirmed_state();auto transaction=controller->set_sample_rate(*Hertz::from_integer(2000000));if(!transaction||!runtime.progress({0}))return 4;
 auto captured=runtime.backend_capability(*device);if(!captured||!*captured)return 5;auto old=std::move(**captured);captured->reset();
 RecoveryConfig recovery{4,confirmed,true};recovery.confirmed_state.fields[0].value=std::uint32_t{4};if(!device->recover(recovery))return 6;
 for(unsigned n=1;n<8;++n)if(!runtime.progress({n}))return 7;
 if(device->sid()!=4)return 8;
 FieldOutcome late;late.id=SampleRate::id;late.value=*Hertz::from_integer(99000000);late.validity=Validity::known;late.status=FieldStatus::executed;
 if(old.complete(late)||!runtime.progress({8}))return 9;
 if(device->confirmed_state().fields[1].validity!=Validity::known||std::get<Hertz>(device->confirmed_state().fields[1].value)!=*Hertz::from_integer(1000000)||device->sid()!=4)return 10;
 // The old capability cannot publish after Runtime destruction either.
 made->reset();if(old.complete(late))return 11;old={};return 0;
}
