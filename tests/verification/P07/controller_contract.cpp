#include <vita/runtime/transaction/controller.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
static TransactionKey identity(){return {7,{9,1},1,codec::Identifier::short_id(3),codec::Identifier::short_id(2),0};}
static Packet response(codec::Envelope request,AckKind kind,bool cancellation=false,unsigned selected=0,std::int64_t value=2){
    AckRecord a;a.request=request;a.cancellation=cancellation;a.cam.raw=request.command->cam;a.cam.action=(a.cam.raw>>23)&3;a.kind=kind;a.scheduled_or_executed=true;a.selected_mask=selected;
    for(unsigned i=0;i<4;++i)if(selected&(1u<<i)){a.state.fields[i].validity=Validity::known;if(i==1)a.state.fields[i].value=*Hertz::from_integer(value);else a.state.fields[i].value=std::uint32_t{7};}
    Packet p;auto n=encode_response(a,p.bytes);if(!n)std::abort();p.size=*n;return p;
}
static Bytes bytes(const Packet& p){return Bytes{p.bytes}.first(p.size);}
int main(){
    {
        ControllerRegistry<2,128,4> registry;auto rel=registry.register_relationship(identity(),UINT32_MAX);if(!rel)return 1;auto packet=make(false);auto last=registry.track(*rel,packet.view(),{},50'000'000);if(!last||last->envelope.command->message_id!=UINT32_MAX)return 2;
        if(registry.track(*rel,packet.view(),{},50'000'000))return 3;
        auto restart=identity();++restart.binding_generation;++restart.peer.generation;if(registry.register_relationship(restart,1))return 4;
        auto same=registry.register_relationship(identity(),1);if(!same||registry.track(*same,packet.view(),{},50'000'000))return 5;
        restart.stream_id=2;if(!registry.register_relationship(restart,1))return 6;
    }
    {
        ControllerRegistry<2,128,2> registry;auto rel=registry.register_relationship(identity(),42);auto original=make(false,42,3,2,0xa9080000);auto tracked=registry.track(*rel,original.view(),{},50'000'000);if(!tracked)return 7;
        auto cancel=cancellation(42,2,0xa9080000);auto registered=registry.register_cancel(tracked->handle,cancel.view(),{10'000'000},20'000'000);if(!registered||*registered!=CancelRegistration::fresh)return 8;
        auto retry=cancellation(42,2,0xa9080000,9);auto repeated=registry.register_cancel(tracked->handle,retry.view(),{20'000'000},500'000'000);if(!repeated||*repeated!=CancelRegistration::retry||registry.deadline_for(tracked->handle,true)->ns!=30'000'000)return 9;
        auto changed=cancellation(42,1,0xa9080000);if(registry.register_cancel(tracked->handle,changed.view(),{21'000'000},20'000'000))return 10;
        auto cx=response(cancel.view().envelope.envelope,AckKind::execution,true);if(!registry.receive(7,{9,1},bytes(cx),{40'000'000}))return 11;
        auto observer=registry.observer(tracked->handle);if(!observer||!(**observer).cancellation_timed_out()||(**observer).timed_out()||!(**observer).observation().confirms_cancellation||(**observer).observation().confirms_execution||(**observer).observation().success)return 12;
        auto x=response(tracked->envelope,AckKind::execution);if(registry.receive(8,{9,1},bytes(x),{40'000'000})||registry.receive(7,{9,2},bytes(x),{40'000'000}))return 13;
        if(!registry.receive(7,{9,1},bytes(x),{60'000'000}))return 14;
        observer=registry.observer(tracked->handle);if(!observer||!(**observer).timed_out()||!(**observer).timeout_observation()||(**observer).observation().success||!(**observer).observation().confirms_execution)return 15;
        if(!registry.release(tracked->handle))return 16;
        registry.expire({30'049'999'999});if(registry.size()!=1)return 17;registry.expire({30'050'000'000});if(registry.size()!=0)return 18;
    }
    {
        ControllerRegistry<2,128,2> registry;auto rel=registry.register_relationship(identity(),42);auto original=make(false,42,2,2,0xa90c0000);auto tracked=registry.track(*rel,original.view(),{},100'000'000);if(!tracked)return 19;
        auto x=response(tracked->envelope,AckKind::execution);auto s=response(tracked->envelope,AckKind::state,false,2,2);if(!registry.receive(7,{9,1},bytes(x),{1})||!registry.receive(7,{9,1},bytes(s),{2}))return 20;
        auto cancel=cancellation(42,2,0xa90c0000);if(!registry.register_cancel(tracked->handle,cancel.view(),{3},100'000'000))return 21;
        auto cx=response(cancel.view().envelope.envelope,AckKind::execution,true);auto cs=response(cancel.view().envelope.envelope,AckKind::state,true,2,3);if(!registry.receive(7,{9,1},bytes(cx),{4})||!registry.receive(7,{9,1},bytes(cs),{5}))return 22;
        auto old=registry.state_observation(tracked->handle,false),newer=registry.state_observation(tracked->handle,true);
        if(!old||!*old||!newer||!*newer||std::get<Hertz>((**old).state.fields[1].value).q20!=2*(1ll<<20)||std::get<Hertz>((**newer).state.fields[1].value).q20!=3*(1ll<<20))return 23;
        auto wrong=response(tracked->envelope,AckKind::state,false,2,9);if(registry.receive(7,{9,1},bytes(wrong),{6}))return 24;
        auto wrong_field=response(tracked->envelope,AckKind::state,false,1,2);if(registry.receive(7,{9,1},bytes(wrong_field),{6}))return 25;
        auto changed_class=tracked->envelope;changed_class.class_id=codec::ClassId{1,2,3,0};auto wrong_class=response(changed_class,AckKind::execution);if(registry.receive(7,{9,1},bytes(wrong_class),{6}))return 26;
        if(!registry.receive(7,{9,1},bytes(s),{20'000'000'000})||!registry.release(tracked->handle))return 27;
        registry.expire({30'000'000'004});if(registry.size()!=1)return 28;registry.expire({30'000'000'005});if(registry.size())return 29;
    }
    {
        // No-Ack and clock changes do not turn silence into confirmed execution or implicit cancellation.
        ControllerRegistry<1,128,1> registry;auto rel=registry.register_relationship(identity(),42);auto noack=make(false,42,2,2,0xa9000000);auto tracked=registry.track(*rel,noack.view(),{},50'000'000);if(!tracked)return 30;
        timing::ProtocolClock clock;clock.bind({timing::Epoch::gps,true,false,true,0,0,2'000'000'000});clock.observe_pps({0},timing::ProtocolTime{1000,0});clock.observe_pps({10'000'000},timing::ProtocolTime{2000,0});
        if(!registry.advance({49'999'999})||(**registry.observer(tracked->handle)).timed_out())return 31;
        auto unsolicited=response(tracked->envelope,AckKind::execution);if(registry.receive(7,{9,1},bytes(unsolicited),{49'999'999}))return 32;
        if(!registry.advance({50'000'000})||!(**registry.observer(tracked->handle)).timed_out()||(**registry.observer(tracked->handle)).cancellation_timed_out()||registry.deadline_for(tracked->handle,true))return 33;
    }
    return 0;
}
