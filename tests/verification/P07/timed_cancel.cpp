#include <vita/runtime/transaction/manager.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
static Packet timed(timing::ProtocolTime requested,unsigned mode=1){auto p=cancellation(42,2,0xa9080000);auto v=p.view();auto e=v.envelope.envelope;e.command->cam|=mode<<12;e.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,static_cast<std::uint32_t>(requested.seconds),requested.picoseconds};Packet out;auto n=codec::encode_envelope(e,v.envelope.payload,std::nullopt,out.bytes);if(!n)std::abort();out.size=*n;return out;}
static OperationContext now(){OperationContext c;c.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};c.timing=timing::TimingCapabilities::deterministic();return c;}
int main(){
    for(unsigned invalid=0;invalid<4;++invalid){
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.external_retention=true;StateSnapshot initial;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);Engine<1> engine(pool,backend.binding(),initial,options);RetentionStore<4,8192> store(pool);TransactionManager<1,4,8192> manager(engine,store,pool);auto context=now();auto original=make(false,42,2);if(!manager.accept(original.view(),context,{9,1}))return 1;auto budget=pool.used(Resource::duplicate_bytes);
        auto request=timed(invalid==0?timing::ProtocolTime{100,500'000'000}:invalid==1?timing::ProtocolTime{111,0}:timing::ProtocolTime{100,10'000'000'000});if(invalid==2)context.timing={};if(invalid==3)context.clock.uncertainty_ps=1'000'001;
        if(manager.accept(request.view(),context,{9,1})||backend.begins()||pool.used(Resource::cancellation_queue)||pool.used(Resource::cancellation_response)||pool.used(Resource::duplicate_bytes)!=budget)return 2;
    }
    for(unsigned failure=0;failure<3;++failure){
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.external_retention=true;StateSnapshot initial;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);Engine<1> engine(pool,backend.binding(),initial,options);RetentionStore<4,8192> store(pool);TransactionManager<1,4,8192> manager(engine,store,pool);auto context=now();auto original=make(false,42,2);auto a=manager.accept(original.view(),context,{9,1});if(!a||!manager.progress(context))return 3;
        auto request=timed({100,10'000'000'000});auto c=manager.accept(request.view(),context,{9,1});if(!c)return 4;
        context.monotonic={11'000'000};context.clock.time={100,10'001'000'001};
        if(failure==1){context.clock.time={200,0};++context.clock.mapping_generation;}
        if(failure==2)context.clock.state=timing::ClockState::faulted;
        if(!manager.progress(context))return 5;auto result=manager.response(c->token,0);if(!result||!*result||!(**result).partial||(**result).scheduled_or_executed||(**result).timing!=7||backend.pending()!=1||backend.writes())return 6;
        if(!backend.complete_next()||!manager.progress(context)||backend.writes()!=1)return 7;
    }
    return 0;
}
