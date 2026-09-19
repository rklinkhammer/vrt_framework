#include <vita/runtime/transaction/engine.hpp>
#include "sink.hpp"
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;

template<std::size_t N> static Result<Handle> submit(Engine<N>& engine,std::uint32_t raw,const ControlPacket& packet,OperationContext now={}){
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.command=Command{raw,1,Identifier::short_id(2),Identifier::short_id(3)};
    std::array<std::byte,256> wire;auto size=encode_packet(e,packet.freeze(),wire);if(!size)return std::unexpected(size.error());auto decoded=decode_packet(Bytes{wire}.first(*size));if(!decoded)return std::unexpected(decoded.error());return engine.accept(*decoded,now);
}
template<std::size_t N> static bool drain(Engine<N>& engine,VirtualBackend<>& backend,Handle h,OperationContext now={}){
    for(unsigned i=0;i<20;++i){auto done=engine.complete(h);if(!done)return false;if(*done)return true;if(!engine.progress(now))return false;if(backend.pending()&&!backend.complete_next())return false;}return false;
}
int main(){
    StateSnapshot initial;for(unsigned i=0;i<3;++i)initial.fields[i].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(0);
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink effects;
        VirtualRule warning;warning.diagnostics.warnings=precision;backend.set_rule(SampleRate::id,warning);
        VirtualRule bad;bad.resolvable=false;bad.diagnostics.errors=unsupported;backend.set_rule(StateEvent::id,bad);
        Engine<2> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test,effects.binding()});
        ControlPacket p;p.set<ReferencePoint>(1);p.set<SampleRate>(*Hertz::from_integer(1));p.set<StateEvent>(0);
        auto h=submit(engine,0xa91f0000,p);if(!h||!drain(engine,backend,*h))return 1;
        if(backend.writes()!=1||effects.state->count!=1||std::get<std::uint32_t>(engine.state().fields[0].value)!=1||std::get<Hertz>(engine.state().fields[1].value).q20!=0)return 2;
        for(unsigned phase=0;phase<3;++phase){auto ack=engine.take_response(*h);if(!ack||!*ack||static_cast<unsigned>((**ack).kind)!=phase)return 3;if(phase==1&&(!(**ack).partial||(**ack).scheduled_or_executed))return 4;if(phase==2&&((**ack).selected_mask!=7||(**ack).partial||!(**ack).scheduled_or_executed))return 5;}
        if(!engine.release(*h))return 6;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink effects;initial.fields[1].value=*Hertz::from_integer(1000000);
        Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,effects.binding()});
        ControlPacket p;p.configure(0,1);p.set<SampleRate>(*Hertz::from_integer(2000000));auto h=submit(engine,0xa89c0000,p);
        if(!h||!drain(engine,backend,*h)||backend.writes()||backend.begins()||effects.state->count||std::get<Hertz>(engine.state().fields[1].value).q20!=1000000ll*(1<<20))return 7;
        for(unsigned phase=0;phase<3;++phase){auto ack=engine.take_response(*h);if(!ack||!*ack||!(**ack).hypothetical||(**ack).cam.action!=1)return 8;if(phase==2&&std::get<Hertz>((**ack).state.fields[1].value).q20!=2000000ll*(1<<20))return 9;}
    }
    {
        // Releasing A must never discard B's independently READY completion.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;Engine<2> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test});
        ControlPacket p;p.set<ReferencePoint>(5);auto a=submit(engine,0xa9000000,p);if(!a||!drain(engine,backend,*a))return 10;
        p.replace<ReferencePoint>(6);auto b=submit(engine,0xa9000000,p);if(!b||!engine.progress({})||backend.pending()!=1||!backend.complete_next())return 11;
        if(!engine.release(*a)||!engine.progress({})||!*engine.complete(*b)||std::get<std::uint32_t>(engine.state().fields[0].value)!=6)return 12;
    }
    {
        // P=0 prevalidation rejects all fields when one requested write is unresolvable.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VirtualRule bad;bad.resolvable=false;bad.diagnostics.errors=unsupported;backend.set_rule(SampleRate::id,bad);
        Engine<1> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test});ControlPacket p;p.set<ReferencePoint>(7);p.set<SampleRate>(*Hertz::from_integer(2));auto h=submit(engine,0xa1000000,p);
        if(!h||!drain(engine,backend,*h)||backend.writes()||backend.begins())return 13;
    }
    {
        // Recycled lower slot C must not overtake earlier B or interleave B's fields.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;
        Engine<2> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test,sink.binding()});ControlPacket p;p.set<ReferencePoint>(2);
        auto a=submit(engine,0xa9000000,p);if(!a||!drain(engine,backend,*a))return 14;sink.clear();
        p.replace<ReferencePoint>(3);p.set<SampleRate>(*Hertz::from_integer(3));OperationContext bcontext;bcontext.operation=2;auto b=submit(engine,0xa9000000,p,bcontext);if(!b||!engine.release(*a))return 15;
        ControlPacket cpacket;cpacket.set<ReferencePoint>(9);OperationContext ccontext;ccontext.operation=3;auto c=submit(engine,0xa9000000,cpacket,ccontext);if(!c||!drain(engine,backend,*c))return 16;
        if(sink.state->count!=3||sink.state->records[0]->event.source_operation!=2||sink.state->records[1]->event.source_operation!=2||sink.state->records[2]->event.source_operation!=3)return 17;
    }
    {
        // Unexpected P=0 failure reports real prior effects and stops later writes.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;VirtualRule fail;fail.completion=FieldStatus::failed;backend.set_rule(SampleRate::id,fail);
        Engine<1> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test,sink.binding()});ControlPacket p;p.set<ReferencePoint>(7);p.set<SampleRate>(*Hertz::from_integer(2));p.set<StateEvent>(1);
        auto h=submit(engine,0xa1000000,p);if(!h||!drain(engine,backend,*h)||backend.begins()!=2||backend.writes()!=1||sink.state->count!=1)return 18;
        auto outcomes=engine.outcomes(*h);if(!outcomes||(*outcomes)[0].status!=FieldStatus::executed||(*outcomes)[1].status!=FieldStatus::failed||(*outcomes)[2].status!=FieldStatus::not_executed)return 19;
    }
    {
        // Unknown required state faults subsequent live admission; late completion cannot restore it.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;
        Engine<1> engine(pool,backend.binding(),initial,{Profile::iq_generator_v1,sink.binding()});ControlPacket p;p.set<SampleRate>(*Hertz::from_integer(2));auto h=submit(engine,0xa9000000,p);
        if(!h||!engine.progress({}))return 20;auto callback=backend.pending_capability();if(!callback||!callback->synthetic_failure()||!engine.progress({})||!*engine.complete(*h))return 21;
        if(engine.state().fields[1].validity!=Validity::unknown||sink.state->count!=1||sink.state->records[0]->event.outcome.status!=FieldStatus::unknown_effect)return 22;
        if(!engine.release(*h)||submit(engine,0xa9000000,p))return 23;
        auto late=backend.complete_next();if(!late||*late||engine.state().fields[1].validity!=Validity::unknown)return 24;
    }
    {
        // P=1 continues independent fields but never a dependent field after failed prerequisite.
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VirtualRule failed;failed.completion=FieldStatus::failed;backend.set_rule(ReferencePoint::id,failed);VirtualRule dependent;dependent.dependencies=1;backend.set_rule(SampleRate::id,dependent);
        Engine<1> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test});ControlPacket p;p.set<ReferencePoint>(7);p.set<SampleRate>(*Hertz::from_integer(2));p.set<StateEvent>(0);auto h=submit(engine,0xa9000000,p);
        if(!h||!drain(engine,backend,*h)||backend.begins()!=2||backend.writes()!=1)return 25;
        auto outcomes=engine.outcomes(*h);if(!outcomes||(*outcomes)[0].status!=FieldStatus::failed||(*outcomes)[1].status!=FieldStatus::not_executed||(*outcomes)[2].status!=FieldStatus::executed)return 26;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;VerifySink sink;VirtualRule a;a.dependencies=2;backend.set_rule(ReferencePoint::id,a);VirtualRule b;b.dependencies=1;backend.set_rule(SampleRate::id,b);
        Engine<1> engine(pool,backend.binding(),initial,{Profile::generic_virtual_test,sink.binding()});ControlPacket p;p.set<ReferencePoint>(7);p.set<SampleRate>(*Hertz::from_integer(2));if(submit(engine,0xa9000000,p)||backend.begins()||sink.state->occupied)return 27;
        for(std::size_t i=0;i<resource_count;++i)if(pool.used(static_cast<Resource>(i)))return 28;
    }
    return 0;
}
