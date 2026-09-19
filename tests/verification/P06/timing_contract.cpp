#include <vita/runtime/transaction/engine.hpp>
#include <cstdio>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
static Result<Handle> submit(Engine<1>& engine,OperationContext now){
    ControlPacket p;p.set<SampleRate>(*Hertz::from_integer(2));Envelope e;e.type=PacketType::command;e.stream_id=1;e.timestamp={Tsi::gps,Tsf::picoseconds,101,0};e.command=Command{0xa91c1000,1,Identifier::short_id(2),Identifier::short_id(3)};
    std::array<std::byte,128> wire;auto size=encode_packet(e,p.freeze(),wire);if(!size)return std::unexpected(size.error());auto decoded=decode_packet(Bytes{wire}.first(*size));if(!decoded)return std::unexpected(decoded.error());return engine.accept(*decoded,now);
}
int main(){
    StateSnapshot state;state.fields[1].validity=Validity::known;state.fields[1].value=*Hertz::from_integer(1);
    for(unsigned failure=0;failure<4;++failure){
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;Engine<1> engine(pool,backend.binding(),state);
        std::array<timing::Boundary,1> boundary{{{{101,0},1024,1,false,true,0}}};OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();now.boundaries=boundary;
        if(failure==0)now.boundaries={};
        auto h=submit(engine,now);if(!h)return 1;
        if(!engine.progress(now)||backend.begins())return 2;
        now.clock.time={101,0};
        if(failure==1)now.clock.state=timing::ClockState::faulted;
        if(failure==2){now.clock.mapping_generation=2;boundary[0].mapping_generation=1;}
        if(failure==3)boundary[0].committed=true;
        for(unsigned n=0;n<5&&!*engine.complete(*h);++n)if(!engine.progress(now))return 3;
        if(backend.begins()||backend.writes()||!*engine.complete(*h)){std::fprintf(stderr,"failure case %u begins%zu complete%d\n",failure,backend.begins(),int(*engine.complete(*h)));return 4;}
        bool seen_x=false;
        for(;;){auto response=engine.take_response(*h);if(!response)return 5;if(!*response)break;auto ack=**response;if(failure==0&&ack.kind==AckKind::validation&&(ack.timing!=7||ack.time_known))return 15;if(ack.kind==AckKind::execution){seen_x=true;if(!ack.partial||ack.scheduled_or_executed||ack.timing!=7||ack.time_known)return 6;
            std::array<std::byte,256> bytes;auto n=encode_response(ack,bytes);if(!n)return 7;auto parsed=decode_packet(Bytes{bytes}.first(*n),DecodeOptions{RequestContext{0xa91c1000}});if(!parsed||((parsed->envelope.envelope.command->cam>>12)&7)!=7||parsed->envelope.envelope.timestamp.tsi!=Tsi::none)return 8;
        }}if(!seen_x)return 9;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;Engine<1> engine(pool,backend.binding(),state);
        std::array<timing::Boundary,1> boundary{{{{101,0},1024,1,false,true,0}}};OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();now.boundaries=boundary;
        auto h=submit(engine,now);if(!h||!engine.progress(now)||backend.begins())return 10;
        now.clock.time={101,0};if(!engine.progress(now)||backend.begins()!=1)return 11;
        FieldOutcome late;late.id=SampleRate::id;late.status=FieldStatus::executed;late.value=*Hertz::from_integer(2);late.validity=Validity::known;late.actual_time={101,1000001};late.time_known=true;
        if(!backend.complete_next(late)||!engine.progress(now)||!*engine.complete(*h))return 12;
        for(;;){auto r=engine.take_response(*h);if(!r)return 13;if(!*r)break;if((**r).kind==AckKind::execution&&((**r).timing!=7||!(**r).partial||(**r).scheduled_or_executed||!(**r).time_known))return 14;}
    }
    return 0;
}
