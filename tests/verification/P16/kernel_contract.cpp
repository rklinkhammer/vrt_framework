#include <vita/runtime/transaction/engine.hpp>
#include <vita/runtime/context/receiver.hpp>
#include <cassert>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
namespace IQ=vita::profiles::iq;
static StateSnapshot initial(bool tuned){StateSnapshot s;s.profile=tuned?IQ::Profile::frequency_tunable:IQ::Profile::generator_v1;for(auto id:baseline_fields)s.fields[field_index(id)].validity=Validity::known;s.fields[0].value=std::uint32_t{1};s.fields[1].value=*Hertz::from_integer(100'000);s.fields[2].value=std::uint32_t{context::valid_data_enable|context::valid_data_indicator};s.fields[3].value=PayloadFormat{0xa00003cf00000000};if(tuned)s.fields[4]={RFReferenceFrequency::id,*Hertz::from_integer(100'000'000),Validity::known};return s;}
template<class Packet>static auto submit(Engine<2>&engine,const Packet&p,unsigned cam=0xa91c0000){Envelope e;e.type=PacketType::command;e.stream_id=1;e.class_id=ClassId{0xabcdef,2,0x120};e.command=Command{cam,7,Identifier::short_id(2),Identifier::short_id(3)};std::array<std::byte,512> wire{};auto n=encode_packet(e,p.freeze(),wire);assert(n);auto parsed=decode_packet(Bytes{wire}.first(*n));assert(parsed);return engine.accept(*parsed,{});}
static void drain(Engine<2>&engine,VirtualBackend<>&backend,Handle handle){for(unsigned i=0;i<16;++i){assert(engine.progress({}));assert(backend.complete_next());assert(engine.progress({}));auto done=engine.complete(handle);assert(done);if(*done)return;}assert(false);}
int main(){
 static_assert(state_field_capacity==5&&command_field_capacity==4);assert(field_index(RFReferenceFrequency::id)==4&&field_index(Bandwidth::id)==5);
 for(bool tuned:{false,true})for(auto q20:{1'000'000ll*1'048'576,6'000'000'000ll*1'048'576,999'999ll*1'048'576,6'000'000'001ll*1'048'576,100'000'000ll*1'048'576+1}){
  AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.profile=tuned?Profile::iq_frequency_tunable:Profile::iq_generator_v1;Engine<2> engine(pool,backend.binding(),initial(tuned),options);ControlPacket command;assert(command.configure(0x120,2)&&command.set<RFReferenceFrequency>(Hertz{q20}));auto handle=submit(engine,command);assert(handle);drain(engine,backend,*handle);bool valid=tuned&&(q20==1'000'000ll*1'048'576||q20==6'000'000'000ll*1'048'576);assert(backend.writes()==unsigned(valid));assert(engine.state().fields[1].value==SemanticValue{*Hertz::from_integer(100'000)});if(valid)assert(engine.state().fields[4].validity==Validity::known&&std::get<Hertz>(engine.state().fields[4].value).q20==q20);else assert(engine.state().fields[4].validity==(tuned?Validity::known:Validity::absent));
 }
 // Partial execution never authorizes the RF half of a forbidden mixed write.
 for(unsigned cam:{0xa91c0000u,0xa11c0000u}){
  AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.profile=Profile::iq_frequency_tunable;Engine<2> engine(pool,backend.binding(),initial(true),options);ControlPacket command;assert(command.configure(0x120,2)&&command.set<RFReferenceFrequency>(*Hertz::from_integer(100'050'000))&&command.set<SampleRate>(*Hertz::from_integer(200'000)));auto h=submit(engine,command,cam);assert(h);drain(engine,backend,*h);assert(backend.begins()==0&&backend.writes()==0);assert(engine.state().fields[4].value==SemanticValue{*Hertz::from_integer(100'000'000)});
 }
 // State capacity5 does not silently increase command capacity4.
 AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.profile=Profile::iq_frequency_tunable;Engine<2> engine(pool,backend.binding(),initial(true),options);QueryPacket all;assert(all.configure(0x120,0));assert(all.select<ReferencePoint>()&&all.select<RFReferenceFrequency>()&&all.select<SampleRate>()&&all.select<StateEvent>()&&all.select<DataPayloadFormat>());auto too_many=submit(engine,all,0xa0040000);assert(!too_many&&too_many.error().code==ErrorCode::resource_limit&&backend.begins()==0);
 auto v1=initial(false);v1.fields[4]={RFReferenceFrequency::id,*Hertz::from_integer(100'000'000),Validity::known};context::ReceiverHistory<> old_history;assert(!old_history.insert(v1,{1,0},{0})&&old_history.size()==0);
 auto tuned=initial(true);assert(context::required_known(tuned));tuned.fields[4].validity=Validity::unknown;assert(!context::required_known(tuned));context::ReceiverHistory<> history(1,Tsi::gps,1,IQ::Profile::frequency_tunable);assert(history.insert(tuned,{1,0},{0}));assert(history.resolve({1,0},{0}).confidence!=context::Confidence::known);
 for(auto q:{0ll,1'000'000ll*1'048'576-1,6'000'000'000ll*1'048'576+1,100'000'000ll*1'048'576+1}){auto bad=initial(true);bad.fields[4].value=Hertz{q};assert(!validate_snapshot(bad));context::ReceiverHistory<> receiver(1,Tsi::gps,1,IQ::Profile::frequency_tunable);assert(!receiver.insert(bad,{1,0},{0})&&receiver.size()==0);}
 Engine<2> mismatched(pool,backend.binding(),initial(false),options);ControlPacket rf;assert(rf.configure(0x120,2)&&rf.set<RFReferenceFrequency>(*Hertz::from_integer(100'050'000)));auto wrong=submit(mismatched,rf);assert(!wrong&&backend.begins()==0);
 // Backend adjustment and completion cannot bypass the RF profile predicate.
 for(bool adjustment:{false,true}) {
  AdmissionPool credits(AdmissionPool::reference_capacities()); VirtualBackend<> device;
  if(adjustment){VirtualRule rule;rule.adjusted=Hertz{100'000'000ll*1'048'576+1};assert(device.set_rule(RFReferenceFrequency::id,rule));}
  Engine<2> target(credits,device.binding(),initial(true),options);
  auto h=submit(target,rf);assert(h);assert(target.progress({}));
  if(adjustment){drain(target,device,*h);assert(device.begins()==0);assert(target.state().fields[4].validity==Validity::known);}
  else {assert(device.pending()==1);FieldOutcome actual;actual.id=RFReferenceFrequency::id;actual.status=FieldStatus::executed;actual.validity=Validity::known;actual.value=Hertz{100'000'000ll*1'048'576+1};auto done=device.complete_next(actual);assert(done&&*done);assert(target.progress({}));assert(target.state().fields[4].validity==Validity::unknown);auto outcomes=target.outcomes(*h);assert(outcomes&&outcomes->size()==4&&(*outcomes)[0].status==FieldStatus::unknown_effect);}
 }

}
