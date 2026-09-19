#include "../P10/runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace verify_p10;
struct Held {
 memory::RetentionQuota consumer{1},global{1};std::optional<memory::RetainedRx> payload;MetadataSnapshot metadata;unsigned packets=0;
 static void receive(void*p,const BorrowedSignalRx&s)noexcept {auto& held=*static_cast<Held*>(p);++held.packets;if(held.payload)return;auto retained=s.retain(held.consumer,held.global);if(!retained)std::abort();held.payload=std::move(*retained);held.metadata=s.metadata;}
 bool intact()const {if(!payload||metadata.confidence!=Confidence::known||std::get<std::uint32_t>(metadata.state.fields[0].value)!=1||std::get<Hertz>(metadata.state.fields[1].value)!=*Hertz::from_integer(1000000))return false;auto bytes=payload->fragment(0);return bytes&&bytes->size()==1024&&(*bytes)[0]==std::byte{0x40}&&(*bytes)[1]==std::byte{0};}
};
int main(){Held held;
 {
  using Runtime=VitaRuntime<1,4,32,65536>;auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;auto config=stream_config();config.receiver={&held,Held::receive,nullptr};auto device=runtime.add_controllee(config);
  if(!device||!runtime.observe_pps({0},{1000,0})||!device->start()||!runtime.progress({0})||!held.intact())return 2;
  RecoveryConfig recovery{4,device->confirmed_state(),true};recovery.confirmed_state.fields[0].value=std::uint32_t{4};if(!device->recover(recovery))return 3;
  for(unsigned n=1;n<8;++n)if(!runtime.progress({n}))return 4;
  if(!runtime.progress({256000})||held.packets!=2||!held.intact())return 5;
 }
 if(!held.intact())return 6;
 held.payload.reset();
 return 0;
}
