#include <vita/runtime/transaction/manager.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
static StateSnapshot initial(){StateSnapshot s;s.fields[0].validity=Validity::known;s.fields[1].validity=Validity::known;s.fields[1].value=*Hertz::from_integer(1);return s;}
static Packet timed_original(){auto p=make(false,42,2,2,0xa91f0000);auto parsed=p.view();auto e=parsed.envelope.envelope;e.command->cam|=1u<<12;e.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,100,10'000'000'000};Packet out;auto n=codec::encode_envelope(e,parsed.envelope.payload,std::nullopt,out.bytes);if(!n)std::abort();out.size=*n;return out;}
static OperationContext context(std::uint64_t ms,std::span<const timing::Boundary> boundaries){OperationContext now;now.monotonic={ms*1'000'000};now.clock={timing::ClockState::locked,{100,ms*1'000'000'000},0,1,true,true,timing::Epoch::gps};now.boundaries=boundaries;now.timing=timing::TimingCapabilities::deterministic();return now;}
int main(){
    std::array<timing::Boundary,1> boundary{{{{100,10'000'000'000},10000,1,false,true,0}}};auto original=timed_original();
    for(unsigned after=0;after<2;++after){
        auto capacities=AdmissionPool::reference_capacities();capacities.need(Resource::ordinary_queue,1);AdmissionPool pool(capacities);VirtualBackend<> backend;EngineOptions options;options.external_retention=true;Engine<1> engine(pool,backend.binding(),initial(),options);RetentionStore<4,16384> store(pool);TransactionManager<1,4,16384> manager(engine,store,pool);
        auto request=manager.accept(original.view(),context(0,boundary),{9,1});if(!request||request->kind!=DuplicateKind::fresh)return 1;
        auto active=manager.accept(original.view(),context(1,boundary),{9,1});if(!active||active->kind!=DuplicateKind::active||!manager.release(active->token))return 2;
        if(!manager.progress(context(9,boundary))||backend.begins())return 3;
        if(after){if(!manager.progress(context(10,boundary))||backend.begins()!=1||!backend.complete_next()||!manager.progress(context(10,boundary)))return 4;}
        auto cancellation_packet=cancellation(42,2);auto cancelled=manager.accept(cancellation_packet.view(),context(after?11:9,boundary),{9,1});if(!cancelled||!cancelled->token.cancellation)return 5;
        auto invalid_subset=cancellation(42,1);auto conflict=manager.accept(invalid_subset.view(),context(after?11:9,boundary),{9,1});if(conflict||conflict.error().code!=ErrorCode::identity_conflict)return 6;
        if(!manager.progress(context(after?11:9,boundary)))return 7;
        auto cx=manager.response(cancelled->token,0);if(!cx||!*cx||!(**cx).cancellation||(**cx).kind!=AckKind::execution||(**cx).partial!=bool(after)||(**cx).scheduled_or_executed==bool(after))return 8;
        std::array<std::byte,256> encoded;auto n=encode_response(**cx,encoded,3);if(!n)return 9;auto decoded=codec::decode_packet(Bytes{encoded}.first(*n),codec::DecodeOptions{codec::RequestContext{0xa90f0000}});if(!decoded||!decoded->envelope.envelope.cancel)return 10;
        for(std::size_t i=0;i<decoded->fields.size();++i){auto d=decoded->fields[i].diagnostic();if(!d||(*d&0x1ff80000u))return 11;}
        if(!manager.progress(context(10+after,boundary))||backend.writes()!=after)return 12;
        auto ox=manager.response(request->token,1);if(!ox||!*ox||(**ox).cancellation||(**ox).partial==bool(after)||(**ox).scheduled_or_executed!=bool(after))return 13;
        const auto timestamp=(**cx).time;auto retry=cancellation(42,2,0xa90f0000,15);auto replay=manager.accept(retry.view(),context(20,boundary),{9,1});if(!replay||replay->kind!=DuplicateKind::replay)return 14;auto historical=manager.response(replay->token,0);if(!historical||!*historical||(**historical).time!=timestamp||backend.writes()!=after)return 15;
    }
    {
        // S10: first field executed, second field running but still reversible.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.external_retention=true;options.profile=Profile::generic_virtual_test;Engine<1> engine(pool,backend.binding(),initial(),options);RetentionStore<4,16384> store(pool);TransactionManager<1,4,16384> manager(engine,store,pool);
        auto p=make(false);auto admitted=manager.accept(p.view(),{}, {9,1});if(!admitted||!manager.progress({})||!backend.complete_next()||!manager.progress({})||backend.begins()!=2)return 16;
        auto stale=backend.pending_capability();if(!stale)return 17;auto cancel=cancellation();auto c=manager.accept(cancel.view(),{}, {9,1});if(!c||!manager.progress({}))return 18;
        auto cx=manager.response(c->token,0);if(!cx||!*cx||!(**cx).partial||!(**cx).scheduled_or_executed)return 19;
        auto ox=manager.response(admitted->token,1);if(!ox||!*ox||!(**ox).partial||(**ox).scheduled_or_executed||backend.writes()!=1)return 20;
        FieldOutcome late;late.id=SampleRate::id;late.status=FieldStatus::executed;late.validity=Validity::known;late.value=*Hertz::from_integer(99);if(stale->complete(late)||std::get<Hertz>(engine.state().fields[1].value).q20!=(1ll<<20))return 21;
        stale.reset();auto fresh=make(false,43,2,3);if(manager.accept(fresh.view(),{}, {9,1})||!engine.faulted()||engine.state().fields[1].validity!=Validity::unknown||backend.writes()!=1)return 22;
    }
    return 0;
}
