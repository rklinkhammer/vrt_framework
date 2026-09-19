#include <vita/runtime/transaction/engine.hpp>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
template<std::size_t N,class Packet> static Result<Handle> submit(Engine<N>& engine,const Packet& packet,Envelope e,OperationContext now={}){
    std::array<std::byte,256> bytes;auto n=encode_packet(e,packet.freeze(),bytes);if(!n)return std::unexpected(n.error());auto decoded=decode_packet(Bytes{bytes}.first(*n));if(!decoded)return std::unexpected(decoded.error());return engine.accept(*decoded,now);
}
static Envelope envelope(std::uint32_t cam){Envelope e;e.type=PacketType::command;e.stream_id=1;e.command=Command{cam,42,Identifier::short_id(2),Identifier::short_id(3)};return e;}
int main(){
    for(unsigned known=0;known<3;++known){
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;StateSnapshot initial;initial.fields[0].value=std::uint32_t{4};initial.fields[1].value=*Hertz::from_integer(5);if(known>0)initial.fields[0].validity=Validity::known;if(known>1)initial.fields[1].validity=Validity::known;
        Engine<1> engine(pool,backend.binding(),initial);QueryPacket q;q.select<ReferencePoint>();q.select<SampleRate>();auto h=submit(engine,q,envelope(0xa01c0000));if(!h||!engine.progress({})||!*engine.complete(*h))return 1;
        for(unsigned phase=0;phase<3;++phase){auto r=engine.take_response(*h);if(!r||!*r||static_cast<unsigned>((**r).kind)!=phase)return 2;if(phase==1&&((**r).partial||(**r).scheduled_or_executed))return 3;if(phase==2&&((**r).partial!=(known<2)||(**r).scheduled_or_executed!=(known>0)||(**r).selected_mask!=(known==0?0:known==1?1:3)))return 4;}
        if(backend.begins()||backend.writes())return 5;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;StateSnapshot initial;initial.fields[1].value=*Hertz::from_integer(1);initial.fields[1].validity=Validity::known;Engine<2> engine(pool,backend.binding(),initial);
        OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();std::array<timing::Boundary,1> bounds{{{{101,0},1024,1,false,true,0}}};now.boundaries=bounds;
        ControlPacket change;change.set<SampleRate>(*Hertz::from_integer(2));auto timed=envelope(0xa9001000);timed.timestamp={Tsi::gps,Tsf::picoseconds,101,0};auto pending=submit(engine,change,timed,now);if(!pending)return 6;
        QueryPacket q;q.select<SampleRate>();auto query=submit(engine,q,envelope(0xa0040000),now);if(!query||!engine.progress(now)||backend.begins()||!*engine.complete(*query)||*engine.complete(*pending))return 7;
        auto ack=engine.take_response(*query);if(!ack||!*ack||std::get<Hertz>((**ack).state.fields[1].value).q20!=(1<<20)||(**ack).time!=timing::ProtocolTime{100,0})return 8;
        if(!engine.release(*query))return 9;
        change.replace<SampleRate>(*Hertz::from_integer(3));auto immediate=submit(engine,change,envelope(0xa9000000),now);if(!immediate||!engine.progress(now)||backend.begins()!=1||!backend.complete_next()||!engine.progress(now)||!*engine.complete(*immediate)||*engine.complete(*pending))return 10;
        if(std::get<Hertz>(engine.state().fields[1].value).q20!=3*(1<<20))return 11;
    }
    return 0;
}
