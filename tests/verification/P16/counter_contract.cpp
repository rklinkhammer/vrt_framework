#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transport;
constexpr Capabilities caps(){Capabilities c;c.reserved_control_slots=8;c.reserved_cancellation_slots=2;return c;}
struct Tap {
 using Loop=adapters::loopback::Loopback<32,128,128>;
 std::optional<Loop> loop;bool reject_context=true,drain=false;unsigned rejected=0,count=0;
 struct Saved{TxToken token;Bytes header;std::array<std::byte,64> original{};std::size_t size=0;};std::array<Saved,24> saved{};
 static constexpr std::size_t storage_bytes=sizeof(Loop)+32*QuiescenceGuard::metadata_bytes()+8192;
 static Result<TransportBinding> create(void* p,HostBindings host)noexcept {
  auto& self=*static_cast<Tap*>(p);auto owner=std::shared_ptr<void>(&self,[](void*){});self.loop.emplace(host.rx_data,host.rx_control,host.rx_cancellation,host.admission,host.routes,host.counters,caps());
  return TransportBinding{owner,&self,storage_bytes,32,caps(),
   [](void* p,TxSubmission&& submission)noexcept->std::expected<TxToken,RejectedSubmission>{auto& tap=*static_cast<Tap*>(p);auto bytes=submission.storage.segment(0);assert(bytes&&bytes->size()>=4);const unsigned type=std::to_integer<unsigned>((*bytes)[0])>>4;
    if(type==4&&tap.reject_context){++tap.rejected;Error e{ErrorCode::capacity_exhausted};e.retryable=true;return std::unexpected(RejectedSubmission{e,std::move(submission)});}
    const bool data=type==1;Bytes header=*bytes;if(data){assert(tap.count<tap.saved.size());assert(((std::to_integer<unsigned>(header[1]))&15)==(tap.count&15));}
    auto accepted=tap.loop->try_send(std::move(submission));if(!accepted)std::fprintf(stderr,"adapterreject type%u code%u stage%u\n",type,unsigned(accepted.error().error.code),unsigned(accepted.error().error.stage));if(accepted&&data){auto& record=tap.saved[tap.count++];record.token=*accepted;record.header=header;record.size=header.size();assert(record.size<=record.original.size());std::memcpy(record.original.data(),header.data(),header.size());}return accepted;},
   [](void*p)noexcept->Result<bool>{auto& tap=*static_cast<Tap*>(p);if(!tap.drain)return false;return tap.loop->progress_next();},
   [](void*p,TxToken t)noexcept{return static_cast<Tap*>(p)->loop->outstanding(t);},
   [](void*p)noexcept{static_cast<Tap*>(p)->loop->close();},
   [](void*p,TxToken t)noexcept->Result<void>{return static_cast<Tap*>(p)->loop->prove_quiescent(t);},
   [](void*,std::span<const Association>,bool)noexcept->Result<void>{return {};},
   [](void*p)noexcept{static_cast<Tap*>(p)->loop.reset();}};
 }
 void verify()const{for(unsigned i=0;i<count;++i){assert(loop->outstanding(saved[i].token));assert(saved[i].header.size()==saved[i].size&&std::memcmp(saved[i].header.data(),saved[i].original.data(),saved[i].size)==0);}}
};
int main(){Tap tap;auto config=profiles::iq::lab::config(0xabcdef);auto pools=profiles::iq::lab::pools();assert(config&&pools);config->transport={&tap,Tap::storage_bytes,32,caps(),Tap::create};auto made=VitaRuntime<1,4,32,65536>::create(*config,std::move(*pools));assert(made);StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.sample_rate=100000;stream.ip_mtu=100;stream.profile=profiles::iq::Profile::frequency_tunable;auto device=(*made)->add_controllee(stream);assert(device);assert((*made)->observe_pps({0},{1000,0})&&device->start());
 for(std::uint64_t now:{0u,110000u,220000u,330000u}){auto result=(*made)->progress({now});if(!result)std::fprintf(stderr,"now%llu error%u retry%d accepted%u\n",(unsigned long long)now,unsigned(result.error().code),result.error().retryable,tap.count);assert(result||result.error().retryable);}assert(tap.rejected>=2&&tap.count==0);tap.reject_context=false;
 for(std::uint64_t now=440000;now<=1'540'000;now+=110000){auto result=(*made)->progress({now});if(!result)std::fprintf(stderr,"now%llu error%u retry%d accepted%u\n",(unsigned long long)now,unsigned(result.error().code),result.error().retryable,tap.count);assert(result||result.error().retryable);tap.verify();}assert(tap.count>=4);tap.drain=true;for(unsigned i=0;i<32;++i){auto progressed=tap.loop->progress_next();assert(progressed);if(!*progressed)break;}for(unsigned i=0;i<tap.count;++i)assert(!tap.loop->outstanding(tap.saved[i].token));made->reset();assert(!tap.loop);
}
