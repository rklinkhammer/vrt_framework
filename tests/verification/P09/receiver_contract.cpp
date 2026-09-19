#include <vita/runtime/context/receiver.hpp>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace vita::memory;
struct Backing{alignas(64)std::array<std::byte,64> bytes{};};
struct Consumer{RetentionQuota local{1},global{1};RetainedRx held;MetadataSnapshot snapshot;unsigned delivered=0,dropped=0;bool retain_failed=false;
 static void deliver(void* p,const BorrowedSignalRx& rx)noexcept{auto& c=*static_cast<Consumer*>(p);++c.delivered;c.snapshot=rx.metadata;auto kept=rx.retain(c.local,c.global);if(kept)c.held=std::move(*kept);else c.retain_failed=true;}
 static void drop(void* p,Confidence)noexcept{++static_cast<Consumer*>(p)->dropped;}
 ReceiverBinding binding(){return{this,deliver,drop};}};
static StateSnapshot known(){StateSnapshot s;for(auto id:baseline_fields)s.fields[field_index(id)].validity=Validity::known;s.fields[1].value=*Hertz::from_integer(1000000);s.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator};s.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return s;}
int main(){
 auto backing=std::make_shared<Backing>();backing->bytes[0]=std::byte{42};BufferSpec spec{backing,backing->bytes.data(),64,1,64,MemoryDomain::cpu};auto pool=ExternalPool::create(std::span{&spec,1});if(!pool)return 1;
 RxEnvelope rx;auto lease=pool->acquire({64,64});if(!lease||!lease->set_size(4))return 2;auto index=rx.add_buffer(std::move(*lease));if(!index||!rx.append_payload({*index,0,4}))return 3;
 {
  RetentionQuota waiting_global(0);Consumer c;ContextReceiver<> receiver(waiting_global,c.binding());if(!receiver.history().insert(known(),{10,0},{0}))return 4;
  if(!receiver.receive_data(rx,{10,0},1,{0})||c.delivered!=1||c.held.fragment_count()!=1||waiting_global.active())return 5;
  if(!receiver.receive_data(rx,{10,1},1,{1})||c.delivered!=2||!c.retain_failed||receiver.waiting())return 6;
 }
 {
  RetentionQuota global(64);Consumer c;ContextReceiver<> receiver(global,c.binding());
  for(unsigned i=0;i<64;++i)if(!receiver.receive_data(rx,{20,i},1,{0}))return 7;
  if(receiver.waiting()!=64||global.active()!=64||receiver.receive_data(rx,{20,64},1,{0}))return 8;
  receiver.progress({9999999});if(receiver.waiting()!=64||c.dropped)return 9;
  receiver.progress({10000000});if(receiver.waiting()||global.active()||c.dropped!=64||c.delivered)return 10;
 }
 {
  RetentionQuota global(64);Consumer c;ContextReceiver<> receiver(global,c.binding());
  if(!receiver.receive_data(rx,{30,0},1,{0})||!receiver.history().insert(known(),{30,0},{9999999}))return 11;
  receiver.progress({9999999});if(receiver.waiting()||global.active()||c.delivered!=1||c.held.fragment_count()!=1)return 12;
  if(!receiver.detach(2,codec::Tsi::gps)||receiver.receive_data(rx,{30,0},1,{10000000}))return 13;
  if(c.snapshot.confidence!=Confidence::known||(*c.held.fragment(0))[0]!=std::byte{42})return 14;
 }
 {
  RetentionQuota global(64);Consumer c;ContextReceiver<> receiver(global,c.binding());
  if(!receiver.receive_data(rx,{40,0},1,{0})||!receiver.history().insert(known(),{40,0},{10000000}))return 15;
  receiver.progress({10000000});if(c.delivered||c.dropped!=1||receiver.waiting())return 16;
 }
 {
  RetentionQuota global(0);Consumer c;ContextReceiver<> missing(global,c.binding(),1,codec::Tsi::gps,true);
  if(missing.receive_data(rx,{50,0},1,{0})||c.delivered)return 17;
  ContextReceiver<> fixed(global,c.binding(),1,codec::Tsi::gps,true,{},PayloadFormat{0x200003cf00000000ULL});
  if(!fixed.receive_data(rx,{50,0},1,{0})||c.delivered!=1||c.snapshot.confidence==Confidence::known||c.snapshot.state.fields[1].validity==Validity::known||c.snapshot.state.fields[3].validity!=Validity::known)return 18;
 }
 return 0;
}
