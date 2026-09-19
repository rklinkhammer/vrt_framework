#include <vita/adapters/loopback/loopback.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
using namespace vita;using namespace vita::runtime;using namespace vita::memory;using namespace vita::adapters::loopback;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"check failed at line %d: %s\n",__LINE__,#x);return 1;}}while(false)
struct Backing{alignas(64)std::array<std::byte,1024> bytes{};unsigned returns{};};
static void returned(void* p,std::size_t)noexcept{++static_cast<Backing*>(p)->returns;}
using Transport=Loopback<8,4,4>;
struct Sink{unsigned calls{};std::array<unsigned,32> counts{},types{};Transport* transport{};std::optional<TxToken> nested;bool nested_rejected{};
 static void receive(void* p,const codec::PacketView& view,const RxEnvelope&)noexcept{
    auto& s=*static_cast<Sink*>(p);s.counts[s.calls]=view.envelope.envelope.packet_count;s.types[s.calls]=static_cast<unsigned>(view.envelope.envelope.type);++s.calls;
    if(s.nested){auto token=*s.nested;s.nested.reset();auto r=s.transport->progress(token);s.nested_rejected=!r&&r.error().code==ErrorCode::would_deadlock;}
 }};
static TxSubmission make(ExternalPool& pool,CompletionArena<16>& arena,CounterRegistry<4>& counts,PeerSession peer,CounterKey key,std::uint64_t id,Fault fault={},bool cancel=false){
    auto lease=pool.acquire({64,64});if(!lease)std::abort();auto bytes=lease->writable_bytes();
    codec::Envelope e;e.type=key.type;e.stream_id=key.stream_id;e.packet_count=*counts.next(key);e.cancel=cancel;
    Result<std::size_t> n=std::unexpected(Error{ErrorCode::invalid_state});
    if(key.type==codec::PacketType::signal){const std::array<std::byte,4> payload{};n=codec::encode_envelope(e,payload,std::nullopt,*bytes);}
    else{e.command=codec::Command{cancel?0x09080000U:0x00040000U,static_cast<std::uint32_t>(id)};
        if(cancel){CancelPacket p;p.select<SampleRate>();n=codec::encode_packet(e,p.freeze(),*bytes);}
        else{QueryPacket p;p.select<SampleRate>();n=codec::encode_packet(e,p.freeze(),*bytes);}}
    if(!n||!lease->set_size(*n))std::abort();TxStorage storage;if(!storage.append(std::move(*lease),0,*n))std::abort();
    auto token=arena.reserve(id);if(!token)std::abort();return {std::move(storage),std::move(*token),peer,key,fault};
}
int main(){
    auto tx=std::make_shared<Backing>(),data=std::make_shared<Backing>(),control=std::make_shared<Backing>(),cancel_rx=std::make_shared<Backing>();
    BufferSpec a{tx,tx->bytes.data(),64,16,64,MemoryDomain::cpu,0,returned,tx.get()},b{data,data->bytes.data(),64,16,64},c{control,control->bytes.data(),64,16,64},d{cancel_rx,cancel_rx->bytes.data(),64,16,64};
    auto txpool=ExternalPool::create(std::span{&a,1}),datapool=ExternalPool::create(std::span{&b,1}),controlpool=ExternalPool::create(std::span{&c,1}),cancelpool=ExternalPool::create(std::span{&d,1});CHECK(txpool&&datapool&&controlpool&&cancelpool);
    PeerSession peer{5,1};CounterKey signal{9,1,codec::PacketType::signal},command{9,1,codec::PacketType::command};Sink sink;
    RouteRegistry<4> routes;CounterRegistry<4> counters;CHECK(routes.add({RouteKey{peer,1,codec::PacketType::signal},&sink,Sink::receive}));CHECK(routes.add({RouteKey{peer,1,codec::PacketType::command},&sink,Sink::receive}));routes.freeze();CHECK(counters.add(signal)&&counters.add(command));counters.freeze();
    AdmissionRequest limits;limits.need(Resource::completion,16).need(Resource::data_queue,16).need(Resource::ordinary_queue,4).need(Resource::cancellation_queue,2);
    AdmissionPool admission(limits);CompletionArena<16> completions;Transport transport(*datapool,*controlpool,*cancelpool,admission,routes,counters);sink.transport=&transport;
    Fault reject;reject.synchronous_reject=true;
    auto rejected=transport.try_send(make(*txpool,completions,counters,peer,signal,1,reject));CHECK(!rejected && *counters.next(signal)==0 && sink.calls==0 && rejected.error().submission.storage.segment_count()==1);
    CHECK(completions.state(0)==TicketState::reserved && tx->returns==0);
    rejected.error().submission.fault.synchronous_reject=false;rejected.error().submission.fault.lose=true;
    auto lost=transport.try_send(std::move(rejected.error().submission));CHECK(lost && transport.progress(*lost));CHECK(sink.calls==0 && *counters.next(signal)==1 && tx->returns==1);
    auto loss_result=completions.consume(0);CHECK(loss_result && loss_result->result.status==CompletionStatus::succeeded && !transport.capabilities().completion_is_delivery);
    Fault duplicate;duplicate.duplicate=true;auto doubled=transport.try_send(make(*txpool,completions,counters,peer,signal,2,duplicate));CHECK(doubled && transport.progress(*doubled));CHECK(sink.calls==2 && sink.counts[0]==1 && sink.counts[1]==1 && completions.consume(0));
    Fault fail;fail.fail_completion=true;fail.hold_quiescence=true;const auto before=tx->returns;
    auto failed=transport.try_send(make(*txpool,completions,counters,peer,signal,3,fail));CHECK(failed && transport.progress(*failed));
    auto failure=completions.consume(0);CHECK(failure && failure->result.status==CompletionStatus::failed && tx->returns==before && transport.outstanding()==1);
    CHECK(transport.prove_quiescent(*failed) && tx->returns==before+1 && !transport.prove_quiescent(*failed));
    auto first=transport.try_send(make(*txpool,completions,counters,peer,signal,4));auto second=transport.try_send(make(*txpool,completions,counters,peer,signal,5));CHECK(first&&second);
    sink.nested=*first;CHECK(transport.progress(*second) && sink.nested_rejected && sink.calls==3);CHECK(transport.progress(*first) && sink.calls==4 && sink.counts[2]==4 && sink.counts[3]==3);
    CHECK(completions.scan([](CompletionRecord)noexcept{})==2);
    {
        Loopback<4,4,4> saturated(*datapool,*controlpool,*cancelpool,admission,routes,counters);
        auto d1=saturated.try_send(make(*txpool,completions,counters,peer,signal,6));auto d2=saturated.try_send(make(*txpool,completions,counters,peer,signal,7));CHECK(d1&&d2);
        auto d3=saturated.try_send(make(*txpool,completions,counters,peer,signal,8));CHECK(!d3);
        auto ordinary=saturated.try_send(make(*txpool,completions,counters,peer,command,9));auto cancellation=saturated.try_send(make(*txpool,completions,counters,peer,command,10,{},true));CHECK(ordinary&&cancellation);
        auto calls=sink.calls;CHECK(saturated.progress(*ordinary)&&saturated.progress(*cancellation)&&sink.calls==calls+2&&sink.types[calls]==6&&sink.types[calls+1]==6);
        CHECK(saturated.progress(*d1)&&saturated.progress(*d2));
    }
    completions.scan([](CompletionRecord)noexcept{});
    CHECK(admission.used(Resource::completion)==0&&admission.used(Resource::data_queue)==0&&admission.used(Resource::ordinary_queue)==0&&admission.used(Resource::cancellation_queue)==0);
    return 0;
}
