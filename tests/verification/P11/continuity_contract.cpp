#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace verify_p10;
struct Capture {unsigned count=0;std::uint32_t first=0;static void accept(void*p,const BorrowedSignalRx&s)noexcept {auto& c=*static_cast<Capture*>(p);++c.count;auto b=s.fragment(0);if(!b)std::abort();c.first=(std::uint32_t((*b)[0])<<24)|(std::uint32_t((*b)[1])<<16)|(std::uint32_t((*b)[2])<<8)|std::uint32_t((*b)[3]);}};
int main(){using Runtime=VitaRuntime<1,4,32,65536>;auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;Capture capture;auto config=stream_config();config.ip_mtu=100;config.receiver={&capture,Capture::accept,nullptr};auto device=runtime.add_controllee(config);
 if(!device||!runtime.observe_pps({0},{1000,0})||!device->start()||!runtime.progress({0})||device->timeline().ordinal()!=11)return 2;
 RecoveryConfig recovery{4,device->confirmed_state(),true};recovery.confirmed_state.fields[0].value=std::uint32_t{4};if(!device->recover(recovery))return 3;
 for(unsigned n=1;n<8;++n)if(!runtime.progress({n}))return 4;
 if(device->timeline().ordinal()<11)return 5;
 if(!runtime.progress({11000})||capture.count!=2||device->timeline().ordinal()!=22||capture.first!=0xe782c4df)return 6;
 return 0;
}
