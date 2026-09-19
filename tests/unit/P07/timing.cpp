#include "support.hpp"
Wire timed(bool cancellation,std::uint64_t ps) {
    Wire w;codec::Envelope e;e.type=codec::PacketType::command;e.stream_id=1;e.cancel=cancellation;
    e.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,100,ps};e.command=codec::Command{cancellation?0x090d1000u:0x091f1000u,7};
    Result<std::size_t> size;
    if(cancellation){CancelPacket body;assert(body.select(SampleRate::id));size=codec::encode_packet(e,body.freeze(),w.bytes);}
    else{ControlPacket body;assert(body.set<SampleRate>(*Hertz::from_integer(2)));size=codec::encode_packet(e,body.freeze(),w.bytes);}
    assert(size);w.size=*size;return w;
}
int main(){
    for(bool after:{false,true}) {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.external_retention=true;
        Engine<2> engine(pool,backend.binding(),initial(),options);RetentionStore<4,8192> store(pool);TransactionManager<2,4,8192> manager(engine,store,pool);
        OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();
        std::array<timing::Boundary,1> boundary{{{{100,10'000'000'000},10,1,false,true,0}}};now.boundaries=boundary;
        auto request=timed(false,10'000'000'000);auto original=manager.accept(request.view(),now,{11,1});assert(original);assert(manager.progress(now));assert(backend.begins()==0);
        if(after){now.clock.time.picoseconds=10'000'000'000;now.monotonic.ns=10'000'000;assert(manager.progress(now));assert(backend.complete_next());assert(manager.progress(now));}
        const auto when=after?11'000'000'000ull:9'000'000'000ull;
        auto cancel=timed(true,when);auto accepted=manager.accept(cancel.view(),now,{11,1});assert(accepted);assert(manager.progress(now));assert(!*manager.response(accepted->token,0));
        now.clock.time.picoseconds=when;now.monotonic.ns=when/1000;assert(manager.progress(now));auto response=manager.response(accepted->token,0);assert(response&&*response);assert((**response).scheduled_or_executed==!after);assert((**response).partial==after);
        now.clock.time.picoseconds=12'000'000'000;assert(manager.progress(now));assert(backend.begins()==std::size_t(after)&&backend.writes()==std::size_t(after));
    }
    // Disarm after cutoff is a truthful failure; actual execution still completes.
    AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;backend.set_reversible(false);EngineOptions options;options.external_retention=true;
    Engine<2> engine(pool,backend.binding(),initial(),options);RetentionStore<4,8192> store(pool);TransactionManager<2,4,8192> manager(engine,store,pool);OperationContext now;
    auto request=make();auto original=manager.accept(request.view(),now,{11,1});assert(original);assert(manager.progress(now));auto cancellation=make(true);auto c=manager.accept(cancellation.view(),now,{11,1});assert(c);assert(manager.progress(now));auto response=manager.response(c->token,0);assert(response&&*response&&(**response).partial&&!(**response).scheduled_or_executed);assert(backend.complete_next());assert(manager.progress(now));assert(backend.writes()==1);
}
