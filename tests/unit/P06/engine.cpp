#include <vita/runtime/transaction/engine.hpp>
#include <cassert>
#include <array>
using namespace vita;
using namespace vita::runtime;
using namespace vita::runtime::transaction;
StateSnapshot known_state(){StateSnapshot state;for(auto id:baseline_fields)state.fields[field_index(id)].validity=Validity::known;state.fields[0].value=std::uint32_t{1};state.fields[1].value=*Hertz::from_integer(1'000'000);state.fields[2].value=std::uint32_t{0};state.fields[3].value=PayloadFormat{0x200003cf00000000ull};return state;}
codec::PacketView command(std::array<std::byte,512>& bytes,std::uint32_t cam,bool multi=false,Hertz rate=Hertz{2'000'000ll<<20}){
    codec::Envelope envelope;envelope.type=codec::PacketType::command;envelope.stream_id=1;envelope.command=codec::Command{cam,1};
    ControlPacket control;assert(control.configure(0,(cam>>23)&3));assert(control.set<SampleRate>(rate));if(multi){assert(control.set<ReferencePoint>(10));assert(control.set<StateEvent>(1));}
    auto encoded=codec::encode_packet(envelope,control.freeze(),bytes);assert(encoded);auto parsed=codec::decode_packet(Bytes{bytes}.first(*encoded));assert(parsed);return std::move(*parsed);
}
template<std::size_t N> void drain(Engine<N>& engine,VirtualBackend<>& backend,Handle handle,OperationContext now){for(unsigned i=0;i<32&&!*engine.complete(handle);++i){assert(engine.progress(now));assert(backend.complete_next());assert(engine.progress(now));}assert(*engine.complete(handle));}
int main(){
    AdmissionPool admission(AdmissionPool::reference_capacities());VirtualBackend<> backend;
    assert(backend.set_rule(SampleRate::id,{true,true,{precision,0},Hertz{3'000'000ll<<20}}));assert(backend.set_rule(StateEvent::id,{true,false,{0,range_error}}));
    Engine<4> engine(admission,backend.binding(),known_state(),EngineOptions{Profile::generic_virtual_test});OperationContext now;now.operation=1;
    std::array<std::byte,512> wire{};auto parsed=command(wire,0x091f0000,true);auto accepted=engine.accept(parsed,now);assert(accepted);drain(engine,backend,*accepted,now);
    assert(backend.begins()==1&&backend.writes()==1);assert(std::get<std::uint32_t>(engine.state().fields[0].value)==10);assert(std::get<Hertz>(engine.state().fields[1].value).q20==(1'000'000ll<<20));
    for(auto kind:{AckKind::validation,AckKind::execution,AckKind::state}){auto response=engine.take_response(*accepted);assert(response&&*response&&(**response).kind==kind);if(kind==AckKind::state){assert(!(**response).partial&&(**response).selected_mask==7);}auto encoded=encode_response(**response,wire);assert(encoded);assert(codec::decode_packet(Bytes{wire}.first(*encoded),codec::DecodeOptions{codec::RequestContext{0x091f0000}}));}
    assert(engine.release(*accepted));
    VirtualBackend<> clean;Engine<2> dry(admission,clean.binding(),known_state());auto dry_packet=command(wire,0x0c8f0000,false,Hertz{(2ll<<20)+(1ll<<19)});auto dry_handle=dry.accept(dry_packet,now);assert(dry_handle);drain(dry,clean,*dry_handle,now);assert(clean.begins()==0&&clean.writes()==0);assert(std::get<Hertz>(dry.state().fields[1].value).q20==(1'000'000ll<<20));
    while(auto response=dry.take_response(*dry_handle)){if(!*response)break;assert((**response).hypothetical);}
    assert(dry.release(*dry_handle));
    // No resources -> no backend effects.
    AdmissionPool exhausted(AdmissionRequest{});Engine<1> blocked(exhausted,clean.binding(),known_state());auto request=command(wire,0x09080000);assert(!blocked.accept(request,now));assert(clean.begins()==0);
    // A future timed plan must not acquire execution ownership ahead of an immediate plan.
    {
        VirtualBackend<> ordered;Engine<2> scheduler(admission,ordered.binding(),known_state());
        OperationContext clock;clock.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};clock.timing=timing::TimingCapabilities::deterministic();
        std::array<timing::Boundary,1> boundaries{{{{110,0},1024,1,false,true,0}}};clock.boundaries=boundaries;
        ControlPacket body;assert(body.set<SampleRate>(Hertz{4'000'000ll<<20}));codec::Envelope env;env.type=codec::PacketType::command;env.stream_id=1;env.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,110,0};env.command=codec::Command{0x09081000,7};
        auto size=codec::encode_packet(env,body.freeze(),wire);assert(size);auto packet=codec::decode_packet(Bytes{wire}.first(*size));assert(packet);auto future=scheduler.accept(*packet,clock);assert(future);assert(scheduler.progress(clock));assert(ordered.begins()==0);
        auto immediate_packet=command(wire,0x09080000);auto immediate=scheduler.accept(immediate_packet,clock);assert(immediate);assert(scheduler.progress(clock));assert(ordered.begins()==1);assert(ordered.complete_next());assert(scheduler.progress(clock));assert(*scheduler.complete(*immediate)&&!*scheduler.complete(*future));
        clock.clock.time={110,0};assert(scheduler.progress(clock));assert(ordered.begins()==2);assert(ordered.complete_next());assert(scheduler.progress(clock));assert(*scheduler.complete(*future));
    }
    return 0;
}
