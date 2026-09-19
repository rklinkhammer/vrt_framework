#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace verify_p10;
namespace {
struct Reinit {
 unsigned calls=0;int mode=0;
 static Result<bool> invoke(void* p,const StateSnapshot&) noexcept {
  auto& self=*static_cast<Reinit*>(p);++self.calls;
  if(self.mode<0)return std::unexpected(Error{ErrorCode::callback_failure});
  return self.mode>0;
 }
};
struct Completion {
 unsigned calls=0;LifecycleStatus last{};
 static void invoke(void*p,const LifecycleStatus&s)noexcept {auto& self=*static_cast<Completion*>(p);++self.calls;self.last=s;}
};
}
int main(){
 using Runtime=VitaRuntime<1,4,32,65536>;
 // Input rejection is transactional and cannot synthesize confirmed state.
 {
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;
  auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 2;
  RecoveryConfig recovery;recovery.new_sid=4;recovery.confirmed_state=device->confirmed_state();
  if(device->recover(recovery)||device->sid()!=1||device->metrics().packets)return 3;
  recovery.peer_ready=true;recovery.new_sid=1;
  if(device->recover(recovery)||device->sid()!=1)return 4;
  recovery.new_sid=4;recovery.confirmed_state.fields[0].value=std::uint32_t{4};recovery.confirmed_state.fields[1].validity=Validity::unknown;
  if(device->recover(recovery)||device->sid()!=1)return 5;
  recovery.confirmed_state=device->confirmed_state();recovery.confirmed_state.fields[1].value=std::uint32_t{7};
  if(device->recover(recovery)||device->sid()!=1)return 6;
 }
 // An explicit reinitialization callback may remain pending: neither a new SID
 // nor Data nor a successful completion may appear before it confirms safety.
 {
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 7;auto& runtime=**made;
  auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 8;
  if(!runtime.configure_virtual_backend(*device,SampleRate::id,{},false,false))return 18;
  Reinit reinit;Completion completion;RecoveryConfig recovery{4,device->confirmed_state(),true,&reinit,Reinit::invoke};recovery.confirmed_state.fields[0].value=std::uint32_t{4};
  if(!device->recover(recovery,{&completion,Completion::invoke}))return 9;
  for(unsigned n=0;n<4;++n)if(!runtime.progress({n}))return 10;
  if(!reinit.calls||completion.calls||device->sid()!=1||device->metrics().packets)return 11;
  reinit.mode=1;
  for(unsigned n=4;n<16;++n)if(!runtime.progress({n}))return 12;
  if(device->sid()!=4||completion.calls!=1||completion.last.phase!=LifecyclePhase::running)return 13;
 }
 // A failed device reinitialization cannot promote the proposed association.
 {
  auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 14;auto& runtime=**made;
  auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 15;
  Reinit reinit;reinit.mode=-1;Completion completion;
  RecoveryConfig recovery{4,device->confirmed_state(),true,&reinit,Reinit::invoke};recovery.confirmed_state.fields[0].value=std::uint32_t{4};
  if(!device->recover(recovery,{&completion,Completion::invoke}))return 16;
  for(unsigned n=0;n<8;++n){auto progressed=runtime.progress({n});(void)progressed;}
  if(!reinit.calls||device->sid()!=1||device->metrics().packets||completion.calls!=1||completion.last.phase!=LifecyclePhase::failed)return 17;
 }
 return 0;
}
