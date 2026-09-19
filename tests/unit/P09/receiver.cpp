#include <vita/runtime/context/receiver.hpp>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;std::abort();}void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void* p=nullptr;if(!posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))return p;std::abort();}void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void* p,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::align_val_t) noexcept{std::free(p);}
struct alignas(64) Storage {std::array<std::byte,128> bytes{};};
StateSnapshot known(){StateSnapshot s;s.fields[1]={SampleRate::id,*Hertz::from_integer(1000000),Validity::known};s.fields[3]={DataPayloadFormat::id,PayloadFormat{0x200003cf00000000ULL},Validity::known};s.fields[2]={StateEvent::id,valid_data_enable|valid_data_indicator,Validity::known};return s;}
struct Consumer {memory::RetentionQuota local{1},global{1};memory::RetainedRx retained;std::size_t calls=0,drops=0;bool keep=false,expect_retention_failure=false;
 static void deliver(void* p,const BorrowedSignalRx& view) noexcept{auto& c=*static_cast<Consumer*>(p);++c.calls;assert(view.fragment(0)->front()==std::byte{42});if(c.keep){auto retained=view.retain(c.local,c.global);if(c.expect_retention_failure)assert(!retained);else{assert(retained);c.retained=std::move(*retained);}}}
 static void drop(void* p,Confidence) noexcept{++static_cast<Consumer*>(p)->drops;}
 ReceiverBinding binding(){return {this,deliver,drop};}
};
int main(){
 auto storage=std::make_shared<Storage>();storage->bytes[0]=std::byte{42};memory::BufferSpec spec{storage,storage->bytes.data(),128,1,64};auto pool=memory::ExternalPool::create(std::span(&spec,1));assert(pool);auto lease=pool->acquire({128,64});assert(lease&&lease->set_size(128));memory::RxEnvelope envelope;auto index=envelope.add_buffer(std::move(*lease));assert(index&&envelope.append_payload({*index,0,128}));
 Consumer consumer;memory::RetentionQuota zero(0),waiting_quota(2);ContextReceiver<4,2> immediate(zero,consumer.binding(),1,codec::Tsi::gps);assert(immediate.history().insert(known(),{100,0},{0}));ContextReceiver<4,2> waiting(waiting_quota,consumer.binding(),1,codec::Tsi::gps);
 memory::RetentionQuota other(1);auto before=allocations;
 assert(immediate.receive_data(envelope,{100,0},1,{1}));assert(consumer.calls==1&&zero.active()==0);
 consumer.keep=true;assert(immediate.receive_data(envelope,{100,0},1,{2}));assert(consumer.local.active()==1);
 consumer.expect_retention_failure=true;assert(immediate.receive_data(envelope,{100,0},1,{3}));assert(consumer.calls==3);consumer.retained.reset();consumer.expect_retention_failure=false;
 assert(waiting.receive_data(envelope,{101,0},1,{4}));assert(waiting.waiting()==1&&waiting_quota.active()==1);
 assert(waiting.history().insert(known(),{101,0},{5}));waiting.progress({5});assert(waiting.waiting()==0&&waiting_quota.active()==0&&consumer.retained.fragment(0)->front()==std::byte{42});
 // Clone rollback and independent quota ownership survive original-handle release.
 auto clone=consumer.retained.retain(other,consumer.global);assert(!clone&&other.active()==0);consumer.global.enable(false);assert(!consumer.retained.retain(other,consumer.global));assert(other.active()==0);consumer.global.enable(true);
 consumer.retained.reset();consumer.keep=false;assert(waiting.receive_data(envelope,{104,0},1,{6}));waiting.progress({10000006});assert(consumer.drops==1&&waiting.waiting()==0);
 assert(allocations==before);
}
