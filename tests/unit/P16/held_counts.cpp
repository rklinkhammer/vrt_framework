#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstring>
using namespace vita;
namespace tx = runtime::transport;
using Adapter = adapters::loopback::Loopback<16,128,128>;
struct Probe {
  unsigned rejects=2, count=0;
  std::array<Bytes,16> live{};
  std::array<std::array<std::byte,32>,16> copies{};
};
constexpr tx::Capabilities capabilities(){tx::Capabilities c;c.reserved_control_slots=4;return c;}
struct Owner {
  Adapter adapter; Probe& probe;
  Owner(tx::HostBindings h,Probe& p):adapter(h.rx_data,h.rx_control,h.rx_cancellation,h.admission,h.routes,h.counters,capabilities()),probe(p){}
};
constexpr auto bytes=sizeof(Owner)+16*runtime::QuiescenceGuard::metadata_bytes()+128;
int main(){
 Probe probe;
 auto config=profiles::iq::lab::config(0xabcdef);assert(config);
 config->transport={&probe,bytes,16,capabilities(),[](void*p,tx::HostBindings h)noexcept->Result<tx::TransportBinding>{
   auto owner=std::make_shared<Owner>(h,*static_cast<Probe*>(p));
   return tx::TransportBinding{owner,owner.get(),bytes,16,capabilities(),
    [](void*p,tx::TxSubmission&& submission)noexcept->std::expected<tx::TxToken,tx::RejectedSubmission>{
      auto& o=*static_cast<Owner*>(p);auto& q=o.probe;
      for(unsigned i=0;i<q.count;++i)assert(std::memcmp(q.live[i].data(),q.copies[i].data(),q.live[i].size())==0);
      auto frame=tx::inspect(submission.storage);assert(frame);
      if(frame->envelope.type==codec::PacketType::context&&q.rejects){--q.rejects;Error error{ErrorCode::capacity_exhausted};error.retryable=true;return std::unexpected(tx::RejectedSubmission{error,std::move(submission)});}
      const bool data=frame->envelope.type==codec::PacketType::signal;
      auto header=submission.storage.segment(0);assert(header);
      if(data){assert(frame->envelope.packet_count==q.count%16);assert(header->size()<=32);q.live[q.count]=*header;std::memcpy(q.copies[q.count].data(),header->data(),header->size());}
      auto accepted=o.adapter.try_send(std::move(submission));assert(accepted);
      if(data)++q.count;
      return accepted;
    },
    [](void*)noexcept->Result<bool>{return false;},
    [](void*p,tx::TxToken t)noexcept{return static_cast<Owner*>(p)->adapter.outstanding(t);},
    [](void*p)noexcept{static_cast<Owner*>(p)->adapter.close();},
    [](void*p,tx::TxToken t)noexcept{return static_cast<Owner*>(p)->adapter.prove_quiescent(t);},
    [](void*,std::span<const tx::Association>,bool)noexcept->Result<void>{return {};},
    [](void*p)noexcept{static_cast<Owner*>(p)->adapter.close();}};
 }};
 auto pools=profiles::iq::lab::pools();assert(pools);auto runtime=VitaRuntime<1,4,32,65536>::create(*config,std::move(*pools));assert(runtime);
 StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.sample_rate=100000;
 auto source=(*runtime)->add_controllee(stream);assert(source);assert((*runtime)->observe_pps({0},{1000,0}));assert(source->start());
 for(std::uint64_t n=0;n<4;++n){auto progressed=(*runtime)->progress({n*2'560'000});assert(progressed||progressed.error().retryable);}
 assert(probe.rejects==0&&probe.count>=3);
 for(unsigned i=0;i<probe.count;++i)assert(std::memcmp(probe.live[i].data(),probe.copies[i].data(),probe.live[i].size())==0);
}
