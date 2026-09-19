#include <vita/runtime/transaction/engine.hpp>
#include <cstdio>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
int main(){
unsigned cases=0;
for(unsigned action=0;action<3;++action)for(unsigned permissions=0;permissions<8;++permissions)for(unsigned requests=0;requests<8;++requests)for(unsigned details=0;details<4;++details)for(unsigned nack=0;nack<2;++nack)for(unsigned quality=0;quality<4;++quality){
    const auto raw=0xa0000000u|(action<<23)|(permissions<<25)|(requests<<18)|(details<<16)|(nack?1u<<22:0);
    Envelope e;e.type=PacketType::command;e.stream_id=1;e.command=Command{raw,42,Identifier::short_id(2),Identifier::short_id(3)};
    std::array<std::byte,256> wire{};Result<std::size_t> size;
    if(action==0){QueryPacket p;p.select<SampleRate>();size=encode_packet(e,p.freeze(),wire);}else{ControlPacket p;p.configure(0,action);p.set<SampleRate>(*Hertz::from_integer(2));size=encode_packet(e,p.freeze(),wire);}
    if(!size)return 1;auto decoded=decode_packet(Bytes{wire}.first(*size));if(!decoded)return 2;
    AdmissionPool admission(AdmissionPool::reference_capacities());VirtualBackend<> backend;
    VirtualRule rule;rule.diagnostics={quality&1?1u<<27:0u,quality&2?1u<<28:0u};if(!backend.set_rule(SampleRate::id,rule))return 3;
    StateSnapshot initial;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);
    Engine<1> engine(admission,backend.binding(),initial,EngineOptions{Profile::generic_virtual_test});OperationContext now;
    auto handle=engine.accept(*decoded,now);if(!handle)return 4;
    for(unsigned n=0;n<8&&!*engine.complete(*handle);++n){if(!engine.progress(now))return 5;if(backend.pending()&&!backend.complete_next())return 6;}
    if(!*engine.complete(*handle))return 7;
    const bool eligible=action==0||((!(quality&1)||(permissions&2))&&(!(quality&2)||(permissions&1)));
    const bool warning=action!=0&&(quality&1);const bool error=action!=0&&((quality&2)||(!eligible&&!warning));
    for(unsigned phase=0;phase<3;++phase){
        bool emit=(requests&(4u>>phase))&&(phase==2||!nack||((phase==1&&action==0)?false:warning||error));
        if(!emit)continue;auto next=engine.take_response(*handle);if(!next||!*next)return 8;const auto& ack=**next;
        if(static_cast<unsigned>(ack.kind)!=phase||ack.hypothetical!=(action==1))return 9;
        bool partial=phase==2?false:(phase==1&&action==0?false:!eligible);
        bool schx=phase==2?true:(phase==1&&action==0?false:eligible);
        if(ack.partial!=partial||ack.scheduled_or_executed!=schx){std::fprintf(stderr,"flags a%u p%u r%u d%u n%u q%u phase%u\n",action,permissions,requests,details,nack,quality,phase);return 10;}
        std::array<std::byte,256> response_wire{};auto encoded=encode_response(ack,response_wire);if(!encoded)return 11;
        auto checked=decode_packet(Bytes{response_wire}.first(*encoded),DecodeOptions{RequestContext{raw}});if(!checked)return 12;
        auto ackcam=checked->envelope.envelope.command->cam;if(((ackcam>>23)&3)!=action)return 13;
        if(phase!=2){
            bool w=phase==1&&action==0?false:warning,er=phase==1&&action==0?false:error;
            if(bool(ackcam&(1u<<17))!=w||bool(ackcam&(1u<<16))!=er)return 14;
            unsigned detail_count=((details&2)&&w?1:0)+((details&1)&&er?1:0);
            if(checked->fields.size()!=detail_count)return 15;
            for(std::size_t fi=0;fi<checked->fields.size();++fi){const auto& f=checked->fields[fi];if(f.bytes.size()!=4||!*f.diagnostic())return 16;}
        }else{
            if(checked->fields.size()!=1)return 17;
            auto expected=action!=0&&eligible?2:1;
            if(std::get<Hertz>(*checked->fields[0].value()).q20!=expected*(1ll<<20))return 18;
        }
    }
    auto surplus=engine.take_response(*handle);if(!surplus||*surplus)return 19;
    if(backend.writes()!=(action==2&&eligible?1u:0u))return 20;
    const auto expected_live=action==2&&eligible?2:1;
    if(std::get<Hertz>(engine.state().fields[1].value).q20!=expected_live*(1ll<<20))return 21;
    if(!engine.release(*handle))return 22;
    if(admission.used(Resource::transaction)||admission.used(Resource::completion)||admission.used(Resource::revision))return 23;
    ++cases;
}
if(cases!=6144)return 24;
return 0;
}
