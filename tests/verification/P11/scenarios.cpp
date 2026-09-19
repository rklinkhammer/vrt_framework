#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace vita::runtime::context;using namespace verify_p10;
struct Receiver {
 unsigned packets=0;std::uint32_t reference=0;Hertz rate{};bool known=false;
 static void accept(void*p,const BorrowedSignalRx& signal)noexcept {
  auto& self=*static_cast<Receiver*>(p);++self.packets;
  self.known=signal.metadata.confidence==Confidence::known;self.reference=std::get<std::uint32_t>(signal.metadata.state.fields[0].value);self.rate=std::get<Hertz>(signal.metadata.state.fields[1].value);
 }
};
int main(){using Runtime=VitaRuntime<1,4,32,65536>;
 auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;Receiver receiver;
 auto config=stream_config();config.receiver={&receiver,Receiver::accept,nullptr};auto device=runtime.add_controllee(config);if(!device)return 2;auto controller=runtime.add_controller(*device);
 if(!controller||!runtime.observe_pps({0},{1000,0})||!device->start()||!runtime.progress({0})||receiver.packets!=1)return 3;
 const auto confirmed=device->confirmed_state();VirtualRule failed;failed.completion=FieldStatus::unknown_effect;
 if(!runtime.configure_virtual_backend(*device,SampleRate::id,failed))return 4;
 auto old=controller->set_sample_rate(*Hertz::from_integer(2000000));if(!old||!runtime.progress({1}))return 5;
 for(unsigned n=1;n<=4;++n){auto progressed=runtime.progress({256000+n-1});(void)progressed;}
 if(device->confirmed_state().fields[1].validity!=Validity::unknown||device->start())return 6;
 const auto stopped_count=receiver.packets;RecoveryConfig recovery{1,confirmed,true};
 if(device->recover(recovery)||device->sid()!=1||receiver.packets!=stopped_count)return 7; // S15
 recovery.new_sid=4;recovery.confirmed_state.fields[0].value=std::uint32_t{4};
 if(!device->recover(recovery))return 8;
 for(unsigned n=5;n<=16;++n)if(!runtime.progress({256000+n-1}))return 9;
 if(device->sid()!=4||device->confirmed_state().fields[1].validity!=Validity::known)return 10;
 if(!runtime.progress({512000})||receiver.packets<=stopped_count||!receiver.known||receiver.reference!=4||receiver.rate!=*Hertz::from_integer(1000000))return 11;
 // Existing handle retains original association outcome rather than resolving a
 // same-slot replacement; it cannot turn the old unknown effect into success.
 auto historical=controller->observation(*old);if(!historical||historical->confirms_execution)return 12;
 return 0;
}
