#include "../../unit/P05/support.hpp"
#include <vita/runtime/transaction/engine.hpp>
using namespace vita;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
using namespace vita::adapters::loopback;
struct Endpoint {
    Engine<2>* engine=nullptr;OperationContext now{};std::optional<Handle> handle;
    ControllerObserver observer{17};std::uint32_t cam=0;std::size_t replies=0;
    static void command(void* p,const codec::PacketView& view,const memory::RxEnvelope&) noexcept {auto& e=*static_cast<Endpoint*>(p);auto h=e.engine->accept(view,e.now);assert(h);e.handle=*h;}
    static void response(void* p,const codec::PacketView& view,const memory::RxEnvelope&) noexcept {auto& e=*static_cast<Endpoint*>(p);assert(e.observer.receive(view));++e.replies;}
    static std::optional<codec::RequestContext> correlate(void* p,const codec::Envelope& e) noexcept {if(!e.command||e.command->message_id!=17)return {};return codec::RequestContext{static_cast<Endpoint*>(p)->cam};}
};
int main(){
    // Generic, explicitly class-omitting in-process fixture. IQ profile permissions remain separate.
    for(bool simulated:{false,true}) {
        auto tx=p05_test::pool(),data=p05_test::pool(),control=p05_test::pool(),cancel=p05_test::pool();
        AdmissionPool admission(AdmissionPool::reference_capacities());VirtualBackend<> backend;
        assert(backend.set_rule(SampleRate::id,{true,true,{precision,0},Hertz{3'000'000ll<<20}}));
        assert(backend.set_rule(StateEvent::id,{true,false,{0,range_error}}));
        StateSnapshot initial;for(auto& f:initial.fields)f.validity=Validity::known;
        initial.fields[0].value=std::uint32_t{1};initial.fields[1].value=Hertz{1'000'000ll<<20};initial.fields[2].value=std::uint32_t{0};initial.fields[3].value=PayloadFormat{0x200003cf00000000ull};
        Engine<2> engine(admission,backend.binding(),initial,EngineOptions{Profile::generic_virtual_test});
        Endpoint endpoint;endpoint.engine=&engine;endpoint.now.operation=17;endpoint.cam=simulated?0x089f0000:0x091f0000;
        RouteRegistry<8> routes;CounterRegistry<8> counters;
        assert(routes.add({{{11,1},1,codec::PacketType::command},&endpoint,Endpoint::command}));
        assert(routes.add({{{22,1},1,codec::PacketType::command},&endpoint,Endpoint::response,Endpoint::correlate}));routes.freeze();
        CounterKey request_key{11,1,codec::PacketType::command},response_key{22,1,codec::PacketType::command};assert(counters.add(request_key));assert(counters.add(response_key));counters.freeze();
        CompletionArena<16> tickets;Loopback<8,8,8> transport(data,control,cancel,admission,routes,counters);
        std::size_t completions=0;
        auto send=[&](memory::BufferLease lease,std::size_t length,CounterKey key,PeerSession peer,std::uint64_t operation){assert(lease.set_size(length));memory::TxStorage storage;assert(storage.append(std::move(lease),0,length));auto ticket=tickets.reserve(operation);assert(ticket);auto accepted=transport.try_send({std::move(storage),std::move(*ticket),peer,key,{}});assert(accepted);assert(transport.progress(*accepted));tickets.scan([&](CompletionRecord r) noexcept {assert(r.result.status==CompletionStatus::succeeded);++completions;});};
        auto lease=tx.acquire({512});assert(lease);auto output=lease->writable_bytes();assert(output);
        ControlPacket body;assert(body.configure(0,simulated?1:2));assert(body.set<ReferencePoint>(10));assert(body.set<SampleRate>(Hertz{2'000'000ll<<20}));assert(body.set<StateEvent>(1));
        codec::Envelope envelope;envelope.type=codec::PacketType::command;envelope.stream_id=1;envelope.packet_count=*counters.next(request_key);envelope.command=codec::Command{endpoint.cam,17};auto encoded=codec::encode_packet(envelope,body.freeze(),*output);assert(encoded);
        send(std::move(*lease),*encoded,request_key,{11,1},1);endpoint.observer.local_send(true);assert(endpoint.handle&&endpoint.observer.observation().unknown_remote_outcome);
        for(unsigned i=0;i<32&&!*engine.complete(*endpoint.handle);++i){assert(engine.progress(endpoint.now));assert(backend.complete_next());assert(engine.progress(endpoint.now));}
        assert(*engine.complete(*endpoint.handle));
        while(true){auto next=engine.take_response(*endpoint.handle);assert(next);if(!*next)break;auto block=tx.acquire({512});assert(block);auto bytes=block->writable_bytes();assert(bytes);auto count=counters.next(response_key);assert(count);auto size=encode_response(**next,*bytes,*count);assert(size);send(std::move(*block),*size,response_key,{22,1},10+endpoint.replies);}
        assert(endpoint.replies==3&&completions==4);assert(*counters.next(request_key)==1&&*counters.next(response_key)==3);
        assert(backend.begins()==(simulated?0:1)&&backend.writes()==(simulated?0:1));assert(std::get<std::uint32_t>(engine.state().fields[0].value)==(simulated?1u:10u));
        assert(std::get<Hertz>(engine.state().fields[1].value).q20==1'000'000ll<<20);assert(engine.release(*endpoint.handle));assert(tx.return_count()==4);
        for(auto observation:endpoint.observer.observations())assert(!observation.success); // Partial S1 and hypothetical S2 never prove whole execution success.
    }
}
